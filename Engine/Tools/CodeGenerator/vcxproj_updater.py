#!/usr/bin/env python3
"""
.vcxproj 파일에 .generated.h 및 .generated.cpp 파일들을 자동으로 추가하고
.vcxproj.filters 파일에 Generated 가상 폴더를 설정합니다.
또한 Source/Runtime 하위 폴더들을 자동으로 AdditionalIncludeDirectories에 추가합니다.
"""

import xml.etree.ElementTree as ET
from pathlib import Path
import sys
import re


def indent_xml(elem, level=0):
    """XML 요소에 들여쓰기 추가 (in-place)"""
    i = "\n" + level * "  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for child in elem:
            indent_xml(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


def update_vcxproj(vcxproj_path: Path, generated_cpp_files: list[Path], generated_h_files: list[Path]) -> bool:
    """
    .vcxproj 파일에 generated 파일들을 추가합니다.

    Args:
        vcxproj_path: .vcxproj 파일 경로
        generated_cpp_files: 생성된 .cpp 파일 경로 리스트
        generated_h_files: 생성된 .h 파일 경로 리스트

    Returns:
        True if file was modified, False otherwise
    """
    if not vcxproj_path.exists():
        print(f"[ERROR] vcxproj not found: {vcxproj_path}")
        return False

    # XML 파싱 (namespace 처리)
    ET.register_namespace('', 'http://schemas.microsoft.com/developer/msbuild/2003')
    tree = ET.parse(vcxproj_path)
    root = tree.getroot()

    ns = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}

    # 기존 ClCompile 항목 찾기
    existing_cpp_files = set()
    for item_group in root.findall('.//ms:ItemGroup', ns):
        for compile_item in item_group.findall('ms:ClCompile', ns):
            include = compile_item.get('Include')
            if include:
                existing_cpp_files.add(Path(include).as_posix())

    # 기존 ClInclude 항목 찾기
    existing_h_files = set()
    for item_group in root.findall('.//ms:ItemGroup', ns):
        for include_item in item_group.findall('ms:ClInclude', ns):
            include = include_item.get('Include')
            if include:
                existing_h_files.add(Path(include).as_posix())

    project_dir = vcxproj_path.parent
    modified = False

    # CPP 파일 추가
    cpp_files_to_add = []
    for gen_file in generated_cpp_files:
        try:
            rel_path = gen_file.relative_to(project_dir)
        except ValueError:
            rel_path = gen_file

        rel_path_str = str(rel_path).replace('/', '\\')

        if rel_path.as_posix() not in existing_cpp_files:
            cpp_files_to_add.append(rel_path_str)

    if cpp_files_to_add:
        # ClCompile ItemGroup 찾기 (없으면 생성)
        compile_group = None
        for item_group in root.findall('.//ms:ItemGroup', ns):
            if item_group.find('ms:ClCompile', ns) is not None:
                compile_group = item_group
                break

        if compile_group is None:
            compile_group = ET.SubElement(root, '{http://schemas.microsoft.com/developer/msbuild/2003}ItemGroup')

        for file_path in cpp_files_to_add:
            compile_elem = ET.SubElement(compile_group, '{http://schemas.microsoft.com/developer/msbuild/2003}ClCompile')
            compile_elem.set('Include', file_path)
            print(f"  [+] Added CPP: {file_path}")
            modified = True

    # Header 파일 추가
    h_files_to_add = []
    for gen_file in generated_h_files:
        try:
            rel_path = gen_file.relative_to(project_dir)
        except ValueError:
            rel_path = gen_file

        rel_path_str = str(rel_path).replace('/', '\\')

        if rel_path.as_posix() not in existing_h_files:
            h_files_to_add.append(rel_path_str)

    if h_files_to_add:
        # ClInclude ItemGroup 찾기 (없으면 생성)
        include_group = None
        for item_group in root.findall('.//ms:ItemGroup', ns):
            if item_group.find('ms:ClInclude', ns) is not None:
                include_group = item_group
                break

        if include_group is None:
            include_group = ET.SubElement(root, '{http://schemas.microsoft.com/developer/msbuild/2003}ItemGroup')

        for file_path in h_files_to_add:
            include_elem = ET.SubElement(include_group, '{http://schemas.microsoft.com/developer/msbuild/2003}ClInclude')
            include_elem.set('Include', file_path)
            print(f"  [+] Added Header: {file_path}")
            modified = True

    if modified:
        # 들여쓰기 적용
        indent_xml(root)
        tree.write(vcxproj_path, encoding='utf-8', xml_declaration=True)
        print(f"[OK] Updated: {vcxproj_path.name}")
        return True
    else:
        print(f"[INFO] No new files to add to {vcxproj_path.name}")
        return False


def update_include_directories(vcxproj_path: Path, source_dir: Path) -> bool:
    """
    Source/Runtime 하위의 모든 폴더를 스캔하여 AdditionalIncludeDirectories를 자동으로 업데이트합니다.

    Args:
        vcxproj_path: .vcxproj 파일 경로
        source_dir: 소스 디렉토리 경로 (예: Source/Runtime)

    Returns:
        True if file was modified, False otherwise
    """
    if not vcxproj_path.exists():
        print(f"[ERROR] vcxproj not found: {vcxproj_path}")
        return False

    if not source_dir.exists():
        print(f"[ERROR] Source directory not found: {source_dir}")
        return False

    project_dir = vcxproj_path.parent

    # Source/Runtime 하위의 모든 폴더 수집 (헤더 파일이 있는 폴더만)
    runtime_dirs = set()
    for header_file in source_dir.rglob("*.h"):
        folder = header_file.parent
        # 상대 경로로 변환
        try:
            rel_folder = folder.relative_to(project_dir)
            runtime_dirs.add(rel_folder)
        except ValueError:
            continue

    if not runtime_dirs:
        print(f"[INFO] No header directories found in {source_dir}")
        return False

    # vcxproj 파일 읽기
    with open(vcxproj_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # AdditionalIncludeDirectories 패턴 찾기
    pattern = r'(<AdditionalIncludeDirectories>)([^<]+)(</AdditionalIncludeDirectories>)'

    modified = False

    def update_includes(match):
        nonlocal modified
        prefix = match.group(1)
        current_dirs = match.group(2)
        suffix = match.group(3)

        # 현재 include 디렉토리들 파싱
        current_list = [d.strip() for d in current_dirs.split(';') if d.strip()]

        # 기존 디렉토리들을 집합으로 변환 (비교용)
        existing_normalized = set()
        for d in current_list:
            # $(ProjectDir)를 제거하고 정규화
            normalized = d.replace('$(ProjectDir)', '').replace('\\', '/').strip('/')
            existing_normalized.add(normalized)

        # 새로운 디렉토리 추가
        new_dirs_added = []
        for runtime_dir in sorted(runtime_dirs):
            normalized = str(runtime_dir).replace('\\', '/')
            if normalized not in existing_normalized:
                # $(ProjectDir) 형식으로 추가
                dir_path = str(runtime_dir).replace('/', '\\')
                vs_path = f"$(ProjectDir){dir_path}"
                current_list.insert(-1, vs_path)  # %(AdditionalIncludeDirectories) 앞에 삽입
                new_dirs_added.append(str(runtime_dir))
                modified = True

        if new_dirs_added:
            for d in new_dirs_added:
                print(f"  [+] Added include dir: {d}")

        return prefix + ';'.join(current_list) + suffix

    new_content = re.sub(pattern, update_includes, content)

    if modified:
        with open(vcxproj_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"[OK] Updated include directories in: {vcxproj_path.name}")
        return True
    else:
        print(f"[INFO] All include directories already present in {vcxproj_path.name}")
        return False


def update_vcxproj_filters(filters_path: Path, generated_cpp_files: list[Path], generated_h_files: list[Path]) -> bool:
    """
    .vcxproj.filters 파일에 Generated 가상 폴더를 설정합니다.
    """
    if not filters_path.exists():
        print(f"[WARN] filters file not found: {filters_path}")
        return False

    ET.register_namespace('', 'http://schemas.microsoft.com/developer/msbuild/2003')
    tree = ET.parse(filters_path)
    root = tree.getroot()

    ns = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}

    # 기존 CPP 파일 찾기
    existing_cpp_files = {}
    for item_group in root.findall('.//ms:ItemGroup', ns):
        for compile_item in item_group.findall('ms:ClCompile', ns):
            include = compile_item.get('Include')
            if include:
                existing_cpp_files[Path(include).as_posix()] = compile_item

    # 기존 Header 파일 찾기
    existing_h_files = {}
    for item_group in root.findall('.//ms:ItemGroup', ns):
        for include_item in item_group.findall('ms:ClInclude', ns):
            include = include_item.get('Include')
            if include:
                existing_h_files[Path(include).as_posix()] = include_item

    # Generated 필터가 있는지 확인
    filter_exists = False
    for item_group in root.findall('.//ms:ItemGroup', ns):
        for filter_elem in item_group.findall('ms:Filter', ns):
            if filter_elem.get('Include') == 'Generated':
                filter_exists = True
                break

    # Generated 필터 추가
    if not filter_exists:
        filter_group = None
        for item_group in root.findall('.//ms:ItemGroup', ns):
            if item_group.find('ms:Filter', ns) is not None:
                filter_group = item_group
                break

        if filter_group is None:
            filter_group = ET.SubElement(root, '{http://schemas.microsoft.com/developer/msbuild/2003}ItemGroup')

        filter_elem = ET.SubElement(filter_group, '{http://schemas.microsoft.com/developer/msbuild/2003}Filter')
        filter_elem.set('Include', 'Generated')
        unique_id = ET.SubElement(filter_elem, '{http://schemas.microsoft.com/developer/msbuild/2003}UniqueIdentifier')
        unique_id.text = '{93995380-89BD-4b04-88EB-625FBE52EBFB}'
        print(f"  [+] Created 'Generated' filter")

    project_dir = filters_path.parent
    modified = False

    # CPP 파일에 필터 설정
    for gen_file in generated_cpp_files:
        try:
            rel_path = gen_file.relative_to(project_dir)
        except ValueError:
            rel_path = gen_file

        rel_path_str = str(rel_path).replace('/', '\\')
        rel_path_key = rel_path.as_posix()

        if rel_path_key in existing_cpp_files:
            compile_item = existing_cpp_files[rel_path_key]
            filter_elem = compile_item.find('ms:Filter', ns)

            if filter_elem is None:
                filter_elem = ET.SubElement(compile_item, '{http://schemas.microsoft.com/developer/msbuild/2003}Filter')
                filter_elem.text = 'Generated'
                modified = True
            elif filter_elem.text != 'Generated':
                filter_elem.text = 'Generated'
                modified = True
        else:
            compile_group = None
            for item_group in root.findall('.//ms:ItemGroup', ns):
                if item_group.find('ms:ClCompile', ns) is not None:
                    compile_group = item_group
                    break

            if compile_group is None:
                compile_group = ET.SubElement(root, '{http://schemas.microsoft.com/developer/msbuild/2003}ItemGroup')

            compile_elem = ET.SubElement(compile_group, '{http://schemas.microsoft.com/developer/msbuild/2003}ClCompile')
            compile_elem.set('Include', rel_path_str)
            filter_elem = ET.SubElement(compile_elem, '{http://schemas.microsoft.com/developer/msbuild/2003}Filter')
            filter_elem.text = 'Generated'
            modified = True
            print(f"  [+] Added CPP to filter: {rel_path_str}")

    # Header 파일에 필터 설정
    for gen_file in generated_h_files:
        try:
            rel_path = gen_file.relative_to(project_dir)
        except ValueError:
            rel_path = gen_file

        rel_path_str = str(rel_path).replace('/', '\\')
        rel_path_key = rel_path.as_posix()

        if rel_path_key in existing_h_files:
            include_item = existing_h_files[rel_path_key]
            filter_elem = include_item.find('ms:Filter', ns)

            if filter_elem is None:
                filter_elem = ET.SubElement(include_item, '{http://schemas.microsoft.com/developer/msbuild/2003}Filter')
                filter_elem.text = 'Generated'
                modified = True
            elif filter_elem.text != 'Generated':
                filter_elem.text = 'Generated'
                modified = True
        else:
            include_group = None
            for item_group in root.findall('.//ms:ItemGroup', ns):
                if item_group.find('ms:ClInclude', ns) is not None:
                    include_group = item_group
                    break

            if include_group is None:
                include_group = ET.SubElement(root, '{http://schemas.microsoft.com/developer/msbuild/2003}ItemGroup')

            include_elem = ET.SubElement(include_group, '{http://schemas.microsoft.com/developer/msbuild/2003}ClInclude')
            include_elem.set('Include', rel_path_str)
            filter_elem = ET.SubElement(include_elem, '{http://schemas.microsoft.com/developer/msbuild/2003}Filter')
            filter_elem.text = 'Generated'
            modified = True
            print(f"  [+] Added Header to filter: {rel_path_str}")

    if modified:
        # 들여쓰기 적용
        indent_xml(root)
        tree.write(filters_path, encoding='utf-8', xml_declaration=True)
        print(f"[OK] Updated: {filters_path.name}")
        return True
    else:
        print(f"[INFO] No changes to {filters_path.name}")
        return False


def main():
    if len(sys.argv) < 3:
        print("Usage: python vcxproj_updater.py <vcxproj-path> <generated-file1> [generated-file2] ...")
        print("Example: python vcxproj_updater.py Mundi.vcxproj Generated/Foo.generated.cpp Generated/Foo.generated.h")
        sys.exit(1)

    vcxproj_path = Path(sys.argv[1])
    all_files = [Path(f) for f in sys.argv[2:]]

    # .cpp와 .h 파일 분리
    generated_cpp_files = [f for f in all_files if f.suffix == '.cpp']
    generated_h_files = [f for f in all_files if f.suffix == '.h']

    print("=" * 60)
    print(" Updating Visual Studio Project Files")
    print("=" * 60)
    print(f" Project: {vcxproj_path}")
    print(f" CPP Files: {len(generated_cpp_files)}")
    print(f" Header Files: {len(generated_h_files)}")
    print()

    # .vcxproj 업데이트
    vcxproj_updated = update_vcxproj(vcxproj_path, generated_cpp_files, generated_h_files)

    # .vcxproj.filters 업데이트
    filters_path = vcxproj_path.with_suffix(vcxproj_path.suffix + '.filters')
    filters_updated = update_vcxproj_filters(filters_path, generated_cpp_files, generated_h_files)

    print()
    print("=" * 60)
    if vcxproj_updated or filters_updated:
        print(" [OK] Project files updated!")
    else:
        print(" [INFO] No changes needed")
    print("=" * 60)


if __name__ == '__main__':
    main()
