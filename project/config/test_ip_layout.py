import configparser
import tempfile
import unittest
from pathlib import Path
from ip_layout import compute_layout

class IpLayoutTest(unittest.TestCase):
    def test_localhost_uses_distinct_ports(self):
        cfg = configparser.ConfigParser()
        cfg.read_string("[topology]\nmode=localhost\nnode_num=2\nhost=127.0.0.1\nproxy_port_start=5000\ndatanode_port_start=17000\ncoordinator_ip=127.0.0.1\nclient_ip=127.0.0.1\n")
        layout = compute_layout(cfg)
        self.assertEqual([n.proxy for n in layout.nodes], ["127.0.0.1:5000", "127.0.0.1:5001"])

    def test_distributed_uses_fixed_ports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "nodes").write_text("10.0.0.11\n10.0.0.12\n")
            ini = root / "cluster.ini"
            ini.write_text("[topology]\nmode=distributed\nnode_num=2\nstorage_hosts_file=nodes\nproxy_port_start=50405\ndatanode_port_start=17600\ncoordinator_ip=10.0.0.2\nclient_ip=10.0.0.1\n")
            cfg = configparser.ConfigParser(); cfg.read(ini)
            layout = compute_layout(cfg, ini)
            self.assertEqual([n.proxy for n in layout.nodes], ["10.0.0.11:50405", "10.0.0.12:50405"])
            self.assertEqual([n.datanode for n in layout.nodes], ["10.0.0.11:17600", "10.0.0.12:17600"])

    def test_distributed_count_must_match(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "nodes").write_text("10.0.0.11\n")
            ini = root / "cluster.ini"
            ini.write_text("[topology]\nmode=distributed\nnode_num=2\nstorage_hosts_file=nodes\nproxy_port_start=50405\ndatanode_port_start=17600\ncoordinator_ip=10.0.0.2\nclient_ip=10.0.0.1\n")
            cfg = configparser.ConfigParser(); cfg.read(ini)
            with self.assertRaisesRegex(ValueError, "does not match node_num"):
                compute_layout(cfg, ini)

if __name__ == "__main__": unittest.main()
