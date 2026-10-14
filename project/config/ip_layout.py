#!/usr/bin/env python3
"""Resolve a flat one-proxy/one-datanode localhost topology."""
from __future__ import annotations
import argparse
import configparser
from dataclasses import dataclass
from pathlib import Path

CONFIG_DIR = Path(__file__).resolve().parent
DEFAULT_INI = CONFIG_DIR / "cluster.ini"

@dataclass(frozen=True)
class NodeEndpoint:
    node_id: int
    proxy: str
    datanode: str

@dataclass(frozen=True)
class NodeLayout:
    client_ip: str
    coordinator_ip: str
    nodes: list[NodeEndpoint]

def load_ini(path: Path) -> configparser.ConfigParser:
    cfg = configparser.ConfigParser()
    if not cfg.read(path, encoding="utf-8"):
        raise FileNotFoundError(path)
    return cfg

def repo_root_from_ini(path: Path) -> Path:
    return path.resolve().parent.parent.parent

def compute_layout(cfg: configparser.ConfigParser, _path: Path | None = None) -> NodeLayout:
    section = cfg["topology"]
    count = section.getint("node_num")
    host = section.get("host", "127.0.0.1").strip()
    proxy_start = section.getint("proxy_port_start")
    datanode_start = section.getint("datanode_port_start")
    proxy_data_start = proxy_start + 1000
    proxy_peer_start = proxy_start + 2000
    datanode_data_start = datanode_start + 500
    if count <= 0:
        raise ValueError("node_num must be positive")
    ranges = {
        "proxy grpc": (proxy_start, proxy_start + count - 1),
        "proxy data": (proxy_data_start, proxy_data_start + count - 1),
        "proxy peer": (proxy_peer_start, proxy_peer_start + count - 1),
        "datanode grpc": (datanode_start, datanode_start + count - 1),
        "datanode data": (datanode_data_start, datanode_data_start + count - 1),
    }
    for name, (lo, hi) in ranges.items():
        if lo < 1 or hi > 65535:
            raise ValueError(f"{name} port range {lo}-{hi} is invalid")
    items = list(ranges.items())
    for i, (left_name, (left_lo, left_hi)) in enumerate(items):
        for right_name, (right_lo, right_hi) in items[i + 1:]:
            if max(left_lo, right_lo) <= min(left_hi, right_hi):
                raise ValueError(f"port ranges overlap: {left_name} and {right_name}")
    nodes = [NodeEndpoint(i, f"{host}:{proxy_start+i}", f"{host}:{datanode_start+i}") for i in range(count)]
    return NodeLayout(section.get("client_ip", host).strip(), section.get("coordinator_ip", host).strip(), nodes)

def write_lines(path: Path, lines: list[str], header: str | None = None) -> None:
    values = ([f"# {header}"] if header else []) + lines
    path.write_text("\n".join(values) + "\n", encoding="utf-8")

def main_cli() -> None:
    parser = argparse.ArgumentParser(description="Resolve flat node endpoints")
    parser.add_argument("--ini", type=Path, default=DEFAULT_INI)
    parser.add_argument("--format", required=True, choices=("node-ips", "proxy-ips", "client-ip", "coordinator-ip", "all-hosts", "nodes"))
    args = parser.parse_args()
    layout = compute_layout(load_ini(args.ini), args.ini)
    if args.format == "client-ip": print(layout.client_ip)
    elif args.format == "coordinator-ip": print(layout.coordinator_ip)
    elif args.format == "nodes":
        for n in layout.nodes: print(n.node_id, n.proxy, n.datanode)
    elif args.format == "proxy-ips":
        for n in layout.nodes: print(n.proxy.split(":", 1)[0])
    elif args.format == "node-ips":
        for n in layout.nodes: print(n.datanode.split(":", 1)[0])
    else:
        for host in dict.fromkeys([layout.client_ip, layout.coordinator_ip] + [n.proxy.split(":",1)[0] for n in layout.nodes]): print(host)

if __name__ == "__main__": main_cli()
