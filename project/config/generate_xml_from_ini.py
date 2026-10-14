#!/usr/bin/env python3
"""Generate flat node topology and synchronized runtime configuration."""
import re
import xml.etree.ElementTree as ET
from pathlib import Path
from ip_layout import compute_layout, load_ini, repo_root_from_ini, write_lines

CONFIG_DIR = Path(__file__).resolve().parent
INI_PATH = CONFIG_DIR / "cluster.ini"
NODE_XML_PATH = CONFIG_DIR / "nodeInformation.xml"
PARAM_XML_PATH = CONFIG_DIR / "parameterConfiguration.xml"
MAIN_CLIENT_CPP = CONFIG_DIR.parent / "run_cpp" / "main_client.cpp"

def write_node_xml(nodes) -> None:
    root = ET.Element("nodes")
    for node in nodes:
        ET.SubElement(root, "node", id=str(node.node_id), proxy=node.proxy, datanode=node.datanode)
    ET.indent(root, space="    ")
    ET.ElementTree(root).write(NODE_XML_PATH, encoding="utf-8", xml_declaration=True)

def update_parameters(node_num: int, coordinator_ip: str) -> None:
    tree = ET.parse(PARAM_XML_PATH)
    root = tree.getroot()
    elem = root.find("NodeNum")
    if elem is None: elem = ET.SubElement(root, "NodeNum")
    elem.text = str(node_num)
    root.find("CoordinatorIP").text = coordinator_ip
    ET.indent(root, space="    ")
    tree.write(PARAM_XML_PATH, encoding="utf-8", xml_declaration=True)

def main() -> None:
    cfg = load_ini(INI_PATH)
    layout = compute_layout(cfg, INI_PATH)
    if len(layout.nodes) < 108:
        raise ValueError("node_num must be at least 108 for the configured LotusLRC (96+4+8) stripe")
    write_node_xml(layout.nodes)
    update_parameters(len(layout.nodes), layout.coordinator_ip)
    repo = repo_root_from_ini(INI_PATH)
    hosts = list(dict.fromkeys([layout.client_ip, layout.coordinator_ip] + [n.proxy.split(":",1)[0] for n in layout.nodes]))
    write_lines(repo / "hosts", hosts, "localhost topology hosts")
    write_lines(repo / "proxy_hosts", [n.proxy.split(":", 1)[0] for n in layout.nodes], "proxy hosts")
    text = MAIN_CLIENT_CPP.read_text(encoding="utf-8")
    text = re.sub(r'(std::string client_ip = ")[^"]+(";)', rf'\g<1>{layout.client_ip}\g<2>', text, count=1)
    MAIN_CLIENT_CPP.write_text(text, encoding="utf-8")
    print(f"Generated {len(layout.nodes)} node pairs in {NODE_XML_PATH}")

if __name__ == "__main__": main()
