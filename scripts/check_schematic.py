"""Check the intended circuit against a freshly exported KiCad XML netlist.

Run kicad-cli sch export netlist --format kicadxml first. This check complements
native ERC: ERC does not know which SAMD21 alternate functions the design needs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def load(path):
    tree = ET.parse(path)
    nets = {}
    pin_net = {}
    for net in tree.findall("./nets/net"):
        name = net.attrib["name"]
        members = {f'{n.attrib["ref"]}.{n.attrib["pin"]}' for n in net.findall("node")}
        nets[name] = members
        for member in members:
            assert member not in pin_net, f"Duplicate pin: {member}"
            pin_net[member] = name
    return tree, nets, pin_net


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--netlist", type=Path, default=ROOT / "reports/schematic-review/netlist.xml")
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--pcb-baseline", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    tree, nets, pin_net = load(args.netlist)
    checks = []

    def require(condition, message):
        if not condition:
            raise AssertionError(message)
        checks.append(message)

    def members(name, expected, exact=False):
        actual = nets.get("/" + name, set())
        required = set(expected.split())
        require(actual == required if exact else required <= actual, f"{name}: correct pin membership")

    expected_mcu = {
        "8": "I2S_DACDAT", "11": "I2S_ADCDAT", "13": "I2S_BCLK", "14": "I2S_LRCLK",
        "15": "I2S_MCLK", "19": "AUDIO_CLK_EN", "21": "CODEC_SDA", "22": "CODEC_SCL",
        "23": "USB_DM", "24": "USB_DP", "26": "MCU_RESET_N", "31": "SWCLK",
        "32": "SWDIO", "9": "3V3", "30": "3V3", "29": "VDDCORE", "10": "GND", "28": "GND",
    }
    for pin, net in expected_mcu.items():
        require(pin_net[f"U1.{pin}"] == "/" + net, f"U1.{pin} -> {net}")
    for pin in set(map(str, range(1, 33))) - expected_mcu.keys():
        require(pin_net[f"U1.{pin}"].startswith("unconnected-"), f"U1.{pin}: unused")

    members("I2S_DACDAT", "U1.8 U16.10", exact=True)
    members("I2S_ADCDAT", "U1.11 U16.9", exact=True)
    members("I2S_BCLK", "U1.13 U16.8", exact=True)
    members("I2S_LRCLK", "U1.14 U16.7", exact=True)
    members("I2S_MCLK", "R73.2 U1.15 U16.11", exact=True)
    require(nets[pin_net["Y1.3"]] == {"Y1.3", "R73.1"}, "12 MHz oscillator output passes through R73")
    members("AUDIO_CLK_EN", "U1.19 Y1.1 R74.1", exact=True)
    members("CODEC_SDA", "U1.21 U16.17 R63.1", exact=True)
    members("CODEC_SCL", "U1.22 U16.16 R64.1", exact=True)
    members("3V3", "U1.9 U1.30 U15.2 U15.4 U16.13 U16.14 U16.31 R63.2 R64.2 Y1.4")
    members("GND", "U1.10 U1.28 U16.12 U16.24 U16.28 U16.33 U16.15 R74.2 Y1.2")
    members("VDDCORE", "U1.29 C101.1", exact=True)
    members("MICBIAS", "U16.32 C96.1 R68.1", exact=True)
    members("MODE", "U16.18 R62.2", exact=True)
    members("GND", "R62.1 C101.2")
    members("USB_DP", "U1.24 R53.2", exact=True)
    members("USB_DM", "U1.23 R54.2", exact=True)
    require(nets[pin_net["R53.1"]] == {"R53.1", "D1.1", "USB2.A6", "USB2.B6"}, "USB D+ connector pair and ESD")
    require(nets[pin_net["R54.1"]] == {"R54.1", "D2.1", "USB2.A7", "USB2.B7"}, "USB D- connector pair and ESD")
    for rp, cp in [("R65", "USB2.A5"), ("R66", "USB2.B5")]:
        require(nets[pin_net[cp]] == {cp, f"{rp}.1"}, f"{cp}: independent CC pull-down")
        members("GND", f"{rp}.2")
    members("MCU_RESET_N", "U1.26 R70.2 R72.1 C102.1 J1.10", exact=True)
    members("RESET_SW", "R72.2 SW1.1", exact=True)
    members("SWCLK", "U1.31 R71.2 J1.4", exact=True)
    members("SWDIO", "U1.32 J1.2", exact=True)
    members("3V3", "J1.1 R70.1 R71.1")
    members("GND", "J1.3 J1.5 J1.9 SW1.2 C102.2")
    for ref in ["C97", "C98", "C99", "C100", "C103"]:
        members("3V3", f"{ref}.1")
        members("GND", f"{ref}.2")
    require(nets[pin_net["U16.23"]] == {"U16.23", "SPK2.1"}, "BTL positive output isolated from ground")
    require(nets[pin_net["U16.25"]] == {"U16.25", "SPK2.2"}, "BTL negative output isolated from ground")
    for pin, cap, jack in [("29", "C86", "4"), ("30", "C87", "3")]:
        require(nets[pin_net[f"U16.{pin}"]] == {f"U16.{pin}", f"{cap}.1"}, f"{cap}: positive terminal faces codec")
        require(nets[pin_net[f"{cap}.2"]] == {f"{cap}.2", f"CN2.{jack}"}, f"{cap}: headphone output AC-coupled")

    if args.baseline:
        _, old_nets, old_pin_net = load(args.baseline)
        old_pins = {p for p in old_pin_net if not p.startswith("U1.")}
        bias_merge = {"R68.1", "U16.32", "C96.1"}
        for pin in sorted(old_pins):
            old_group = old_nets[old_pin_net[pin]] & old_pins
            target = bias_merge if pin in bias_merge else old_group
            actual = nets[pin_net[pin]] & old_pins
            require(actual == target, f"Preserved existing topology: {pin}")
    if args.pcb_baseline:
        baseline = json.loads(args.pcb_baseline.read_text(encoding="utf-8-sig"))
        pcb_hash = hashlib.sha256((ROOT / "pcb/WM8978.kicad_pcb").read_bytes()).hexdigest()
        require(pcb_hash.upper() == baseline["Hash"].upper(), "PCB SHA-256 unchanged")

    values = {c.attrib["ref"]: c.findtext("value") for c in tree.findall("./components/comp")}
    for ref, value in {"R70": "10k", "R71": "1k", "R72": "330R", "R73": "33R", "R74": "100k", "C101": "1uF / 10V"}.items():
        require(values[ref] == value, f"{ref} = {value}")
    result = {"verdict": "pass", "checks_passed": len(checks), "component_count": len(values),
              "source_sha256": hashlib.sha256((ROOT / "pcb/WM8978.kicad_sch").read_bytes()).hexdigest(),
              "pcb_sha256": hashlib.sha256((ROOT / "pcb/WM8978.kicad_pcb").read_bytes()).hexdigest(),
              "checks": checks}
    if args.output:
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(checks)} checks; {len(values)} components")


if __name__ == "__main__":
    main()
