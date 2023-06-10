import re
from os import getenv
from subprocess import check_call

import pexpect
import pytest

MIDI_PORT = getenv("MIDI_PORT", "hw:2,0,0")
MIDI_IN_PORT = getenv("MIDI_IN_PORT", MIDI_PORT)
MIDI_OUT_PORT = getenv("MIDI_OUT_PORT", MIDI_PORT)
# See https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
MIDI_MSGS = {
    # Channel Voice Messages
    "NoteOn": "900102",
    "NoteOn chord": "900102 900304 900506",
    "NoteOff": "800102",
    "NoteOff chord": "800102 800304 800506",
    "Polyhonic key pressure": "A00102",
    "Control Change": "B00102",
    "Program Change": "C001",
    "Channel pressure": "D001",
    "Pitch Bend": "E00102",
    "Time code quarter frame": "F101",
    "Song position pointer": "F20102",
    "Song select": "F301",
    "Tune request": "F6",
    # System Real-Time Messages
    "Timing clock": "F8",
    "Start": "FA",
    "Continue": "FB",
    "Stop": "FC",
    "Active Sensing": "FE",
    "Reset": "FF",
} | {
    f"SysEx {n}B": "F0" + ''.join('%02X' % (i+1) for i in range(n)) + "F7"
    for n in range(25)
}


def send_midi(msg: str):
    check_call(["amidi", "-p", MIDI_OUT_PORT, "-S", msg])


@pytest.fixture
def listener():
    proc = pexpect.spawn(f"amidi -p {MIDI_IN_PORT} -dac", timeout=1)
    yield proc
    proc.terminate(force=True)


@pytest.mark.parametrize("midi", MIDI_MSGS.values(), ids=MIDI_MSGS.keys())
def test_loopback(midi, listener):
    send_midi(midi)
    expected = "\r\n".join(
        re.sub(r"([0-9A-F]{2})", "\\1 ", packet).strip()
        for packet in midi.split(" ")
    )

    try:
        listener.expect(expected)
        errored = False
    except pexpect.exceptions.TIMEOUT:
        errored = True

    # Leverage pytest diff instead of pexpect timeout error
    if errored:
        received = listener.before.decode().strip()
        assert received.splitlines() == expected.splitlines()


@pytest.mark.parametrize("n_packets", range(1, 6))
def test_multiple_packets(n_packets, listener):
    all_packets = list(MIDI_MSGS.values())

    i = 0
    while i < len(all_packets):
        test_loopback(" ".join(all_packets[i:n_packets+i]), listener)
        i += n_packets
