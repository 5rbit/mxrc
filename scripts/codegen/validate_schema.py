#!/usr/bin/env python3
"""
IPC Schema Validation Script (Feature 019)

YAML 스키마 파일을 검증하고 타입 일관성, 접근 권한, Hot Key 제약 등을 확인합니다.

Usage:
    python3 validate_schema.py <schema_file.yaml>
"""

import sys
import yaml
from pathlib import Path
from typing import Dict, List, Any, Set

# 지원되는 기본 타입
BASIC_TYPES = {
    'double', 'float', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t',
    'bool', 'string'
}

# 지원되는 복합 타입 (배열 포함)
ARRAY_TYPE_PATTERN = r'array<(\w+),\s*(\d+)>'

# Hot Key 제약
MAX_HOT_KEYS = 32
MAX_HOT_KEY_SIZE_BYTES = 512  # 64축 모터 데이터 지원


class SchemaValidator:
    """IPC 스키마 검증기"""

    def __init__(self, schema_path: Path):
        self.schema_path = schema_path
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self.schema_data: Dict = {}
        self.custom_types: Set[str] = set()

    def validate(self) -> bool:
        """스키마 전체 검증 수행"""
        if not self._load_schema():
            return False

        self._validate_schema_version()
        self._load_custom_types()
        self._validate_datastore_keys()
        self._validate_eventbus_events()
        self._validate_hot_keys()

        return len(self.errors) == 0

    def _load_schema(self) -> bool:
        """YAML 파일 로드"""
        try:
            with open(self.schema_path, 'r', encoding='utf-8') as f:
                self.schema_data = yaml.safe_load(f)
            return True
        except FileNotFoundError:
            self.errors.append(f"Schema file not found: {self.schema_path}")
            return False
        except yaml.YAMLError as e:
            self.errors.append(f"YAML parsing error: {e}")
            return False

    def _validate_schema_version(self):
        """스키마 버전 검증 및 시맨틱 버저닝 확인"""
        if 'schema_version' not in self.schema_data:
            self.errors.append("Missing 'schema_version' field in schema")
            return

        version_str = self.schema_data['schema_version']

        # 시맨틱 버저닝 형식 검증 (MAJOR.MINOR.PATCH)
        import re
        version_pattern = r'^\d+\.\d+\.\d+$'
        if not re.match(version_pattern, version_str):
            self.errors.append(
                f"Invalid schema_version format: '{version_str}' "
                f"(expected: MAJOR.MINOR.PATCH, e.g., 1.0.0)"
            )
            return

        # 버전 파싱
        major, minor, patch = map(int, version_str.split('.'))

        # 버전 범위 검증 (1.x.x만 지원)
        if major != 1:
            self.warnings.append(
                f"Schema version {version_str} uses MAJOR version {major}. "
                f"Only version 1.x.x is currently tested."
            )

        print(f"✓ Schema version: {version_str}")

    def _load_custom_types(self):
        """사용자 정의 타입 로드 (types 섹션에서)"""
        if 'types' in self.schema_data:
            self.custom_types = set(self.schema_data['types'].keys())

    def _validate_datastore_keys(self):
        """DataStore 키 검증"""
        if 'datastore_keys' not in self.schema_data:
            self.errors.append("Missing 'datastore_keys' section in schema")
            return

        keys = self.schema_data['datastore_keys']
        key_names: Set[str] = set()

        for key_name, key_spec in keys.items():
            # 유일성 검증
            if key_name in key_names:
                self.errors.append(f"Duplicate key name: {key_name}")
            key_names.add(key_name)

            # 필수 필드 검증
            if 'type' not in key_spec:
                self.errors.append(f"Missing 'type' for key: {key_name}")
                continue

            # 타입 일관성 검증
            self._validate_type(key_name, key_spec['type'])

            # 접근 권한 검증
            if 'access' in key_spec:
                self._validate_access_permissions(key_name, key_spec['access'])

    def _validate_eventbus_events(self):
        """EventBus 이벤트 검증"""
        if 'eventbus_events' not in self.schema_data:
            self.warnings.append("Missing 'eventbus_events' section (optional)")
            return

        events = self.schema_data['eventbus_events']
        event_names: Set[str] = set()

        for event_name, event_spec in events.items():
            # 유일성 검증
            if event_name in event_names:
                self.errors.append(f"Duplicate event name: {event_name}")
            event_names.add(event_name)

            # 우선순위 검증
            if 'priority' in event_spec:
                priority = event_spec['priority']
                if priority not in ['LOW', 'NORMAL', 'HIGH', 'CRITICAL']:
                    self.errors.append(
                        f"Invalid priority '{priority}' for event: {event_name}"
                    )

            # TTL 검증
            if 'ttl_ms' in event_spec and event_spec['ttl_ms'] is not None:
                if not isinstance(event_spec['ttl_ms'], (int, float)) or event_spec['ttl_ms'] <= 0:
                    self.errors.append(
                        f"Invalid ttl_ms for event: {event_name} (must be positive)"
                    )

    def _validate_hot_keys(self):
        """Hot Key 제약 검증"""
        if 'datastore_keys' not in self.schema_data:
            return

        hot_keys = [
            (name, spec)
            for name, spec in self.schema_data['datastore_keys'].items()
            if spec.get('hot_key', False)
        ]

        # Hot Key 개수 제한 검증
        if len(hot_keys) > MAX_HOT_KEYS:
            self.errors.append(
                f"Too many hot keys: {len(hot_keys)} (max: {MAX_HOT_KEYS})"
            )

        # Hot Key 크기 제한 검증
        for name, spec in hot_keys:
            size = self._estimate_type_size(spec['type'])
            if size > MAX_HOT_KEY_SIZE_BYTES:
                self.errors.append(
                    f"Hot key '{name}' too large: {size} bytes (max: {MAX_HOT_KEY_SIZE_BYTES})"
                )

    def _validate_type(self, key_name: str, type_str: str):
        """타입 유효성 검증"""
        import re

        # 기본 타입 체크
        if type_str in BASIC_TYPES:
            return

        # 배열 타입 체크 (예: array<double, 64>)
        array_match = re.match(ARRAY_TYPE_PATTERN, type_str)
        if array_match:
            element_type = array_match.group(1)
            if element_type not in BASIC_TYPES and element_type not in self.custom_types:
                self.errors.append(
                    f"Invalid array element type '{element_type}' for key: {key_name}"
                )
            return

        # 사용자 정의 타입 체크 (types 섹션에서 정의된 타입)
        if type_str in self.custom_types:
            return

        self.errors.append(f"Unknown type '{type_str}' for key: {key_name}")

    def _validate_access_permissions(self, key_name: str, access: Dict):
        """접근 권한 일관성 검증"""
        rt_read = access.get('rt_read', False)
        rt_write = access.get('rt_write', False)

        # rt_write가 true이면 rt_read도 true여야 함
        if rt_write and not rt_read:
            self.errors.append(
                f"Inconsistent access for key '{key_name}': "
                f"rt_write=true requires rt_read=true"
            )

    def _estimate_type_size(self, type_str: str) -> int:
        """타입 크기 추정 (bytes)"""
        import re

        # 기본 타입 크기
        type_sizes = {
            'double': 8, 'float': 4,
            'int32_t': 4, 'uint32_t': 4,
            'int64_t': 8, 'uint64_t': 8,
            'bool': 1, 'string': 256,  # string은 가변이지만 최대 256으로 가정
            'Vector3d': 24,  # 3 * double
            'TaskStatus': 4,  # enum (int32_t)
            'HAState': 4,  # enum (int32_t)
        }

        if type_str in type_sizes:
            return type_sizes[type_str]

        # 커스텀 타입은 기본 8바이트로 추정 (enum은 4바이트, struct는 더 클 수 있음)
        if type_str in self.custom_types:
            return 8

        # 배열 타입 (예: array<double, 64>)
        array_match = re.match(ARRAY_TYPE_PATTERN, type_str)
        if array_match:
            element_type = array_match.group(1)
            count = int(array_match.group(2))
            element_size = type_sizes.get(element_type, 8)
            return element_size * count

        return 64  # 기본값

    def print_results(self):
        """검증 결과 출력"""
        if self.errors:
            print("❌ Schema validation FAILED")
            print("\nErrors:")
            for error in self.errors:
                print(f"  - {error}")

        if self.warnings:
            print("\nWarnings:")
            for warning in self.warnings:
                print(f"  - {warning}")

        if not self.errors:
            print("✅ Schema validation PASSED")

            # 통계 출력
            if 'datastore_keys' in self.schema_data:
                total_keys = len(self.schema_data['datastore_keys'])
                hot_keys = sum(
                    1 for spec in self.schema_data['datastore_keys'].values()
                    if spec.get('hot_key', False)
                )
                print(f"\nStatistics:")
                print(f"  - Total DataStore keys: {total_keys}")
                print(f"  - Hot Keys: {hot_keys}/{MAX_HOT_KEYS}")

            if 'eventbus_events' in self.schema_data:
                total_events = len(self.schema_data['eventbus_events'])
                print(f"  - EventBus events: {total_events}")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <schema_file.yaml>")
        sys.exit(1)

    schema_path = Path(sys.argv[1])
    validator = SchemaValidator(schema_path)

    if validator.validate():
        validator.print_results()
        sys.exit(0)
    else:
        validator.print_results()
        sys.exit(1)


if __name__ == '__main__':
    main()
