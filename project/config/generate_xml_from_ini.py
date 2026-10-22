#!/usr/bin/env python3
"""Generate flat node topology and synchronize runtime endpoints."""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Tuple

from ip_layout import compute_layout, load_ini, repo_root_from_ini, write_lines

CONFIG_DIR = Path(__file__).resolve().parent
INI_PATH = CONFIG_DIR / "cluster.ini"
NODE_XML_PATH = CONFIG_DIR / "nodeInformation.xml"
PARAM_XML_PATH = CONFIG_DIR / "parameterConfiguration.xml"
MAIN_CLIENT_CPP = CONFIG_DIR.parent / "run_cpp" / "main_client.cpp"
SUPPORTED_CODE_TYPES = {"AzureLRC", "OptimalLRC", "UniformLRC", "LotusLRC", "UniLRC"}


def _positive_int(root: ET.Element, name: str, code_type: str) -> int:
    elem = root.find(name)
    text = None if elem is None else elem.text
    try:
        value = int(text) if text is not None else 0
    except (TypeError, ValueError) as exc:
        raise ValueError(f"invalid {name} for code type {code_type}: required positive integer, actual={text!r}") from exc
    if value <= 0:
        raise ValueError(f"invalid {name} for code type {code_type}: required positive integer, actual={text!r}")
    return value


def parse_code_parameters(xml_path: Path) -> Tuple[str, int, int, int, int]:
    """Read and validate coding parameters without modifying any file."""
    try:
        root = ET.parse(xml_path).getroot()
    except (OSError, ET.ParseError) as exc:
        raise ValueError(f"invalid parameter XML {xml_path}: {exc}") from exc
    code_elem = root.find("CodeType")
    code_type = "" if code_elem is None or code_elem.text is None else code_elem.text.strip()
    if code_type not in SUPPORTED_CODE_TYPES:
        raise ValueError(f"unsupported code type: actual={code_type!r}, required={sorted(SUPPORTED_CODE_TYPES)}")
    z = _positive_int(root, "z", code_type)
    if code_type == "UniLRC":
        alpha = _positive_int(root, "alpha", code_type)
        k = alpha * z * z - alpha * z
        r = alpha * z
        if k <= 0:
            raise ValueError(f"invalid derived k for code type {code_type}: actual={k}, required>0")
    else:
        k = _positive_int(root, "k", code_type)
        r = _positive_int(root, "r", code_type)
    if code_type == "LotusLRC":
        if z % 2 != 0:
            raise ValueError(f"invalid LotusLRC z: actual={z}, required positive even value")
        if k == 24 and (r != 1 or z != 4):
            raise ValueError(
                f"invalid LotusLRC parameters for k=24: actual r={r}, z={z}, required r=1, z=4"
            )
    return code_type, k, r, z, k + r + z


def validate_node_capacity(node_num: int, code_type: str, required: int) -> None:
    if node_num < required:
        raise ValueError(f"insufficient topology nodes for code type {code_type}: actual={node_num}, required={required}")


def write_node_xml(nodes) -> None:
    root = ET.Element("nodes")
    for node in nodes:
        ET.SubElement(root, "node", id=str(node.node_id), proxy=node.proxy, datanode=node.datanode)
    ET.indent(root, space="    ")
    ET.ElementTree(root).write(NODE_XML_PATH, encoding="utf-8", xml_declaration=True)


def update_parameters(node_num: int, coordinator_ip: str, client_ip: str) -> None:
    tree = ET.parse(PARAM_XML_PATH)
    root = tree.getroot()
    node_elem = root.find("NodeNum")
    if node_elem is None:
        node_elem = ET.SubElement(root, "NodeNum")
    node_elem.text = str(node_num)
    coordinator_elem = root.find("CoordinatorIP")
    if coordinator_elem is None:
        raise ValueError("missing CoordinatorIP in parameterConfiguration.xml")
    coordinator_elem.text = coordinator_ip
    client_elem = root.find("ClientIP")
    if client_elem is None:
        client_elem = ET.SubElement(root, "ClientIP")
    client_elem.text = client_ip
    ET.indent(root, space="    ")
    tree.write(PARAM_XML_PATH, encoding="utf-8", xml_declaration=True)


def main() -> None:
    ini_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else INI_PATH
    cfg = load_ini(ini_path)
    layout = compute_layout(cfg, ini_path)
    code_type, _, _, _, required = parse_code_parameters(PARAM_XML_PATH)
    validate_node_capacity(len(layout.nodes), code_type, required)

    write_node_xml(layout.nodes)
    update_parameters(len(layout.nodes), layout.coordinator_ip, layout.client_ip)
    repo = repo_root_from_ini(INI_PATH)
    hosts = list(dict.fromkeys([layout.client_ip, layout.coordinator_ip] + [n.proxy.split(":", 1)[0] for n in layout.nodes]))
    write_lines(repo / "hosts", hosts, "localhost topology hosts")
    write_lines(repo / "proxy_hosts", [n.proxy.split(":", 1)[0] for n in layout.nodes], "proxy hosts")
    text = MAIN_CLIENT_CPP.read_text(encoding="utf-8")
    text = re.sub(r'(std::string client_ip = ")[^"]+(";)', rf'\g<1>{layout.client_ip}\g<2>', text, count=1)
    MAIN_CLIENT_CPP.write_text(text, encoding="utf-8")
    print(f"Generated {len(layout.nodes)} node pairs in {NODE_XML_PATH}")


if __name__ == "__main__":
    main()
