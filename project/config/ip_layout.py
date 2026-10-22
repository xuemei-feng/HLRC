#!/usr/bin/env python3
"""Resolve one-proxy/one-datanode layouts for localhost or real hosts."""
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

def _read_hosts(path: Path) -> list[str]:
    hosts: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        host = raw.split("#", 1)[0].strip()
        if host:
            hosts.append(host)
    if len(hosts) != len(set(hosts)):
        raise ValueError(f"duplicate storage host in {path}")
    return hosts

def compute_layout(cfg: configparser.ConfigParser, path: Path | None = None) -> NodeLayout:
    section = cfg["topology"]
    mode = section.get("mode", "localhost").strip().lower()
    count = section.getint("node_num")
    proxy_start = section.getint("proxy_port_start")
    datanode_start = section.getint("datanode_port_start")
    if count <= 0:
        raise ValueError("node_num must be positive")

    if mode == "localhost":
        host = section.get("host", "127.0.0.1").strip()
        hosts = [host] * count
        proxy_ports = [proxy_start + i for i in range(count)]
        datanode_ports = [datanode_start + i for i in range(count)]
    elif mode == "distributed":
        ini_path = (path or DEFAULT_INI).resolve()
        host_file = Path(section.get("storage_hosts_file", "storage_nodes"))
        if not host_file.is_absolute():
            host_file = ini_path.parent / host_file
        hosts = _read_hosts(host_file)
        if len(hosts) != count:
            raise ValueError(f"storage host count {len(hosts)} does not match node_num {count}")
        proxy_ports = [proxy_start] * count
        datanode_ports = [datanode_start] * count
    else:
        raise ValueError(f"unsupported topology mode: {mode!r}")

    ranges = {
        "proxy grpc": proxy_start,
        "proxy data": proxy_start + 1000,
        "proxy peer": proxy_start + 2000,
        "datanode grpc": datanode_start,
        "datanode data": datanode_start + 500,
    }
    if any(port < 1 or port > 65535 for port in ranges.values()):
        raise ValueError(f"invalid service port: {ranges}")
    if len(set(ranges.values())) != len(ranges):
        raise ValueError(f"service ports overlap: {ranges}")
    if mode == "localhost":
        high_ports = [proxy_start + count - 1, proxy_start + 1000 + count - 1,
                      proxy_start + 2000 + count - 1, datanode_start + count - 1,
                      datanode_start + 500 + count - 1]
        if any(port > 65535 for port in high_ports):
            raise ValueError("localhost port range exceeds 65535")

    nodes = [NodeEndpoint(i, f"{host}:{proxy_ports[i]}", f"{host}:{datanode_ports[i]}")
             for i, host in enumerate(hosts)]
    client_ip = section.get("client_ip", "127.0.0.1").strip()
    coordinator_ip = section.get("coordinator_ip", "127.0.0.1").strip()
    if not client_ip or not coordinator_ip:
        raise ValueError("client_ip and coordinator_ip must be non-empty")
    return NodeLayout(client_ip, coordinator_ip, nodes)

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
