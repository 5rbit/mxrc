#!/usr/bin/env python3
"""
IPC Schema C++ Code Generator (Feature 019)

YAML 스키마에서 C++ 헤더 및 구현 파일을 자동 생성합니다.

Usage:
    python3 generate_ipc_schema.py <schema_file.yaml> <output_dir>
"""

import sys
import yaml
from pathlib import Path
from jinja2 import Environment, FileSystemLoader, Template
from typing import Dict, List, Any


class IPCCodeGenerator:
    """IPC 스키마 C++ 코드 생성기"""

    def __init__(self, schema_path: Path, output_dir: Path, template_dir: Path):
        self.schema_path = schema_path
        self.output_dir = output_dir
        self.template_dir = template_dir
        self.schema_data: Dict = {}

        # Jinja2 환경 설정
        self.env = Environment(
            loader=FileSystemLoader(str(template_dir)),
            trim_blocks=True,
            lstrip_blocks=True
        )

        # 커스텀 필터 등록
        self.env.filters['cpp_type'] = self._to_cpp_type
        self.env.filters['cpp_default'] = self._to_cpp_default
        self.env.filters['upper'] = str.upper

    def generate(self) -> bool:
        """코드 생성 실행"""
        if not self._load_schema():
            return False

        self.output_dir.mkdir(parents=True, exist_ok=True)

        # DataStore 키 헤더 생성
        if 'datastore_keys' in self.schema_data:
            self._generate_datastore_keys()

        # EventBus 이벤트 헤더 생성
        if 'eventbus_events' in self.schema_data:
            self._generate_eventbus_events()

        # Accessor 구현 생성 (옵션)
        # self._generate_accessor_impl()

        print(f"✅ Code generation completed in: {self.output_dir}")
        return True

    def _load_schema(self) -> bool:
        """YAML 파일 로드"""
        try:
            with open(self.schema_path, 'r', encoding='utf-8') as f:
                self.schema_data = yaml.safe_load(f)
            return True
        except (FileNotFoundError, yaml.YAMLError) as e:
            print(f"❌ Error loading schema: {e}")
            return False

    def _generate_datastore_keys(self):
        """DataStore 키 헤더 파일 생성"""
        template = self.env.get_template('datastore_keys.h.j2')

        keys = self.schema_data['datastore_keys']
        hot_keys = {
            name: spec for name, spec in keys.items()
            if spec.get('hot_key', False)
        }

        output = template.render(
            keys=keys,
            hot_keys=hot_keys,
            schema_version=self.schema_data.get('schema', {}).get('version', '1.0.0')
        )

        output_path = self.output_dir / 'DataStoreKeys.h'
        output_path.write_text(output, encoding='utf-8')
        print(f"  Generated: {output_path}")

    def _generate_eventbus_events(self):
        """EventBus 이벤트 헤더 파일 생성"""
        template = self.env.get_template('eventbus_events.h.j2')

        events = self.schema_data['eventbus_events']

        output = template.render(
            events=events,
            schema_version=self.schema_data.get('schema', {}).get('version', '1.0.0')
        )

        output_path = self.output_dir / 'EventBusEvents.h'
        output_path.write_text(output, encoding='utf-8')
        print(f"  Generated: {output_path}")

    def _to_cpp_type(self, type_str: str) -> str:
        """YAML 타입을 C++ 타입으로 변환"""
        import re

        # 배열 타입 (예: array<double, 64>)
        array_match = re.match(r'array<(\w+),\s*(\d+)>', type_str)
        if array_match:
            element_type = array_match.group(1)
            count = array_match.group(2)
            return f"std::array<{element_type}, {count}>"

        # 복합 타입
        if type_str == 'Vector3d':
            return 'Vector3d'  # 프로젝트에서 정의한 타입 사용

        # 기본 타입
        return type_str

    def _to_cpp_default(self, type_str: str, default_value: Any = None) -> str:
        """기본값을 C++ 코드로 변환"""
        if default_value is None:
            if type_str in ['double', 'float']:
                return '0.0'
            elif 'int' in type_str or type_str == 'uint64_t':
                return '0'
            elif type_str == 'bool':
                return 'false'
            elif type_str == 'string':
                return '""'
            else:
                return '{}'  # 구조체/배열 기본 초기화

        # 배열 기본값 처리
        if isinstance(default_value, list):
            elements = ', '.join(str(v) for v in default_value)
            return f"{{{elements}}}"

        return str(default_value)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <schema_file.yaml> <output_dir>")
        sys.exit(1)

    schema_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    # 템플릿 디렉토리는 스크립트와 동일 위치의 templates/
    script_dir = Path(__file__).parent
    template_dir = script_dir / 'templates'

    if not template_dir.exists():
        print(f"❌ Template directory not found: {template_dir}")
        sys.exit(1)

    generator = IPCCodeGenerator(schema_path, output_dir, template_dir)

    if generator.generate():
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
