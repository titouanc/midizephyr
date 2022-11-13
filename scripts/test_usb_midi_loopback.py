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
    # System Common Messages
    "SysEx empty": "F0F7",
    "SysEx 1B": "F001F7",
    "SysEx 2B": "F00102F7",
    "SysEx 3B": "F0010203F7",
    "SysEx 4B": "F001020304F7",
    "SysEx 5B": "F00102030405F7",
    "SysEx 10B": "F00102030405060708090AF7",
    "SysEx empty x2": "F0F7 F0F7",
    "SysEx 1B x2": "F001F7 F001F7",
    "SysEx 2B x2": "F00102F7 F00102F7",
    "SysEx 3B x2": "F0010203F7 F0010203F7",
    "SysEx 4B x2": "F001020304F7 F001020304F7",
    "SysEx 5B x2": "F00102030405F7 F00102030405F7",
    "SysEx 10B x2": "F00102030405060708090AF7 F00102030405060708090AF7",
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
    expected = midi.replace(" ", "\r\n")

    try:
        listener.expect(expected)
        errored = False
    except pexpect.exceptions.TIMEOUT:
        errored = True

    # Leverage pytest diff instead of pexpect timeout error
    if errored:
        received = listener.before.strip().decode()
        assert received.splitlines() == expected.splitlines()


def test_loopback_all_at_once(listener):
    whole_midi = " ".join(MIDI_MSGS.values())
    test_loopback(whole_midi, listener)
