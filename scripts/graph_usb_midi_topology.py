# graph_usb_midi_topology.py:
#
#    Create a graphical representation of the USB-MIDI topology to illustrate
#    the MIDIStreaming routing.
#
#    Read USB descriptors (in the same format as `lsusb -v`) on stdin, and
#    print out a graph in Graphviz .dot format.
#
#    For example, to see the topology of all your MIDI devices:
#    > lsusb -v | python graph_usb_midi_topology.py | dot -Tpng > midi.png

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, Optional


@dataclass
class Descriptor:
    type_: str
    properties: Dict[str, str] = field(default_factory=dict)


def parse_usb_descriptors(lines: Iterable[str]) -> Iterable[Descriptor]:
    descriptor = None
    for line in lines:
        line = line.strip()
        if not line:
            continue

        try:
            if " Descriptor" in line:
                if descriptor is not None:
                    yield descriptor
                descriptor = Descriptor(line.split("Descriptor")[0].strip())
            elif descriptor is not None and '  ' in line:
                name, value = line.split('  ', 1)
                descriptor.properties[name.strip()] = value.strip()
        except Exception as err:
            raise SyntaxError(line) from err
    if descriptor is not None:
        yield descriptor


def usb_string(value: str, default: Optional[str]) -> str:
    if value == "0":
        return value if default is None else default
    return value.split(' ', 1)[1]


def midistreaming_to_dot(descriptors: Iterable[Descriptor]) -> None:
    connectors = []
    ext = []
    emb = []
    ep = []
    relations = []
    dev_id = ""
    dev_name = ""

    def jack_to_dot(desc: Descriptor) -> None:
        jack_id = desc.properties['bJackID']
        label = "\\n".join([f"#{jack_id}", usb_string(desc.properties["iJack"], "")])

        is_embedded = "Embedded" in desc.properties["bJackType"]
        is_output = "OUT" in desc.properties["bDescriptorSubtype"]

        node_name = f"dev{dev_id}jack{jack_id}"
        if is_embedded:
            if is_output:
                emb.append(f"{node_name} [label=\"{label}\", fillcolor=yellow]")
            else:
                emb.append(f"{node_name} [label=\"{label}\", fillcolor=magenta]")
        else:
            if is_output:
                connectors.append(f"{node_name}_conn [shape=rect, label=\"OUTPUT\", fillcolor=lightblue]")
                ext.append(f"{node_name} [label=\"{label}\", fillcolor=blue]")
                relations.append(f"{node_name} -> {node_name}_conn [color=darkblue]")
            else:
                connectors.append(f"{node_name}_conn [shape=rect, label=\"INPUT\", fillcolor=lightgreen]")
                ext.append(f"{node_name} [label=\"{label}\", fillcolor=green]")
                relations.append(f"{node_name}_conn -> {node_name} [color=darkgreen]")

        for pname, pvalue in desc.properties.items():
            if pname.startswith('baSourceID'):
                color = "darkgreen" if is_embedded else "darkblue"
                relations.append(f"dev{dev_id}jack{pvalue} -> dev{dev_id}jack{jack_id} [color={color}]")

    def endpoint_to_dot(desc: Descriptor, name: str) -> None:
        ep_id = int(name.split('  ', 1)[0].replace('0x', ''), 16)
        from_host = "OUT" in name
        ep.append(f"dev{dev_id}ep{ep_id} [label=\"{name}\", color=red, shape=rect]")

        for pname, pvalue in desc.properties.items():
            if pname.startswith('baAssocJackID'):
                if from_host:
                    relations.append(f"dev{dev_id}ep{ep_id} -> dev{dev_id}jack{pvalue} [color=darkblue]")
                else:
                    relations.append(f"dev{dev_id}jack{pvalue} -> dev{dev_id}ep{ep_id} [color=darkgreen]")

    def print_midi_dev():
        print(f"""
            subgraph cluster_dev{dev_id} {{
                label="{dev_name}";

                subgraph dev{dev_id}_connectors {{
                    rank=source;
                    {';'.join(connectors)}
                }}

                subgraph cluster_dev{dev_id}_usb_midi {{
                    style=filled;
                    color=lightgrey;
                    label="MIDIStreaming";
                    subgraph dev{dev_id}_ext {{
                        rank=source;
                        label="External jacks";
                        {';'.join(ext)}
                    }}
                    subgraph dev{dev_id}_emb {{
                        rank=sink;
                        fillcolor=grey;
                        label="Embedded jacks";
                        {';'.join(emb)}
                    }}
                }}

                subgraph cluster_dev{dev_id}_ep {{
                    rank=sink;
                    label="USB";
                    {';'.join(ep)}
                }}

                {' '.join(relations)}
            }}
            """)
        connectors.clear()
        ext.clear()
        emb.clear()
        ep.clear()
        relations.clear()

    def analyze_descriptors():
        nonlocal dev_name, dev_id
        last_endpoint_name = ""
        for desc in descriptors:
            try:
                if desc.type_ == "Device":
                    if ep:
                        print_midi_dev()
                    dev_name = " ".join([
                        usb_string(desc.properties['iManufacturer'], ""),
                        usb_string(desc.properties['iProduct'], ""),
                    ])
                    dev_id = "".join([
                        desc.properties["idVendor"].split(' ', 1)[0],
                        desc.properties["idProduct"].split(' ', 1)[0],
                    ])
                elif desc.type_ == "MIDIStreaming Interface" and "JACK" in desc.properties["bDescriptorSubtype"]:
                    jack_to_dot(desc)
                elif desc.type_ == "Endpoint":
                    last_endpoint_name = desc.properties["bEndpointAddress"]
                elif desc.type_ == "MIDIStreaming Endpoint":
                    endpoint_to_dot(desc, last_endpoint_name)
            except Exception as err:
                raise ValueError(desc) from err

        if ep:
            print_midi_dev()

    print("""digraph midistreaming { node[style=filled];""")
    analyze_descriptors()
    print("}")


if __name__ == "__main__":
    from sys import stdin
    midistreaming_to_dot(parse_usb_descriptors(stdin))
