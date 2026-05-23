import pytest
import ctypes
import struct
import socket


# Simulate the DNS resolver address list structure mirroring the C code behavior
MAXNS = 3  # Typical IRCD_MAXNS value
ADDRLEN = 128  # sizeof(struct irc_ssaddr) - typical size

class IRCResolverSimulator:
    """Simulates the irc_nsaddr_list array and irc_nscount behavior from reslib.c"""
    
    def __init__(self, max_nameservers=MAXNS, addr_element_size=ADDRLEN):
        self.max_nameservers = max_nameservers
        self.addr_element_size = addr_element_size
        self.irc_nscount = 0
        # Allocate fixed-size array
        self.irc_nsaddr_list = bytearray(max_nameservers * addr_element_size)
        # Canary values to detect overflow
        self.canary_before = b'\xDE\xAD\xBE\xEF' * 4
        self.canary_after = b'\xCA\xFE\xBA\xBE' * 4
        self._buffer = bytearray(len(self.canary_before) + len(self.irc_nsaddr_list) + len(self.canary_after))
        self._buffer[:len(self.canary_before)] = self.canary_before
        self._buffer[len(self.canary_before):len(self.canary_before)+len(self.irc_nsaddr_list)] = self.irc_nsaddr_list
        self._buffer[len(self.canary_before)+len(self.irc_nsaddr_list):] = self.canary_after
    
    def safe_add_nameserver(self, ai_addr: bytes, ai_addrlen: int) -> bool:
        """
        Safe version: validates bounds before copying.
        Returns True if the operation is safe, False if it would cause overflow.
        """
        # INVARIANT 1: irc_nscount must be within array bounds
        if self.irc_nscount >= self.max_nameservers:
            return False
        
        # INVARIANT 2: ai_addrlen must not exceed element size
        if ai_addrlen > self.addr_element_size:
            return False
        
        # INVARIANT 3: ai_addrlen must be positive
        if ai_addrlen <= 0:
            return False
        
        # INVARIANT 4: actual data length must match declared length
        if len(ai_addr) < ai_addrlen:
            return False
        
        # Safe copy
        offset = len(self.canary_before) + (self.irc_nscount * self.addr_element_size)
        self._buffer[offset:offset + ai_addrlen] = ai_addr[:ai_addrlen]
        self.irc_nscount += 1
        return True
    
    def canaries_intact(self) -> bool:
        """Check that canary values haven't been corrupted"""
        before_ok = self._buffer[:len(self.canary_before)] == self.canary_before
        after_ok = self._buffer[len(self.canary_before)+len(self.irc_nsaddr_list):] == self.canary_after
        return before_ok and after_ok
    
    def get_array_bounds(self):
        return self.max_nameservers, self.addr_element_size


# Test payloads: (description, list_of_(ai_addr, ai_addrlen) tuples, max_ns_override, element_size_override)
ADVERSARIAL_PAYLOADS = [
    # (description, entries_to_add, max_ns, element_size)
    
    # Overflow: more nameservers than array can hold
    ("too_many_nameservers_ipv4",
     [(socket.inet_aton("1.2.3.4") + b'\x00' * 12, 16)] * 10,
     3, 128),
    
    # Overflow: exactly at boundary (should succeed for first 3, fail for rest)
    ("exactly_at_boundary",
     [(socket.inet_aton("192.168.1.1") + b'\x00' * 12, 16)] * 3,
     3, 128),
    
    # Overflow: one past boundary
    ("one_past_boundary",
     [(socket.inet_aton("10.0.0.1") + b'\x00' * 12, 16)] * 4,
     3, 128),
    
    # Oversized ai_addrlen: larger than element size
    ("oversized_addrlen",
     [(b'\x41' * 256, 256)],
     3, 128),
    
    # Oversized ai_addrlen: exactly element size (boundary)
    ("addrlen_equals_element_size",
     [(b'\x42' * 128, 128)],
     3, 128),
    
    # Oversized ai_addrlen: one byte over element size
    ("addrlen_one_over_element_size",
     [(b'\x43' * 129, 129)],
     3, 128),
    
    # Zero addrlen
    ("zero_addrlen",
     [(b'\x00' * 16, 0)],
     3, 128),
    
    # Negative-like addrlen (large unsigned value)
    ("max_addrlen",
     [(b'\xFF' * 16, 65535)],
     3, 128),
    
    # Combined: too many + oversized
    ("too_many_and_oversized",
     [(b'\xAA' * 512, 512)] * 10,
     3, 128),
    
    # IPv6 address (28 bytes) - valid size
    ("valid_ipv6_addr",
     [(b'\x00' * 28, 28)] * 2,
     3, 128),
    
    # IPv6 address overflow count
    ("ipv6_overflow_count",
     [(b'\x00' * 28, 28)] * 5,
     3, 128),
    
    # Empty address data
    ("empty_addr_data",
     [(b'', 0)],
     3, 128),
    
    # Mismatched: declared length larger than actual data
    ("declared_larger_than_actual",
     [(b'\x01\x02\x03\x04', 16)],
     3, 128),
    
    # Single entry with max array size 1
    ("single_slot_two_entries",
     [(socket.inet_aton("127.0.0.1") + b'\x00' * 12, 16),
      (socket.inet_aton("127.0.0.2") + b'\x00' * 12, 16)],
     1, 128),
    
    # Attack: shellcode-like payload
    ("shellcode_like_payload",
     [(b'\x90' * 64 + b'\xCC' * 64, 128)] * 5,
     3, 128),
    
    # Attack: format string-like data
    ("format_string_payload",
     [(b'%s%s%s%s%n%n%n%n' + b'\x00' * 112, 128)] * 4,
     3, 128),
    
    # Attack: null bytes mixed with data
    ("null_byte_injection",
     [(b'\x00' * 128, 128)] * 4,
     3, 128),
    
    # Attack: maximum count with minimum size
    ("max_count_min_size",
     [(b'\x01', 1)] * 100,
     3, 128),
]


@pytest.mark.parametrize("payload", ADVERSARIAL_PAYLOADS, ids=[p[0] for p in ADVERSARIAL_PAYLOADS])
def test_dns_resolver_array_bounds_invariant(payload):
    """
    Invariant: The DNS resolver address list (irc_nsaddr_list) must never be written
    beyond its allocated bounds. Specifically:
    1. irc_nscount must never exceed MAXNS (array capacity) before a write
    2. ai_addrlen must never exceed the size of a single array element
    3. Memory outside the array must never be corrupted
    4. The operation must either succeed safely or be rejected - never corrupt memory
    """
    description, entries, max_ns, element_size = payload
    
    simulator = IRCResolverSimulator(max_nameservers=max_ns, addr_element_size=element_size)
    
    successful_writes = 0
    rejected_writes = 0
    
    for ai_addr, ai_addrlen in entries:
        result = simulator.safe_add_nameserver(ai_addr, ai_addrlen)
        
        if result:
            successful_writes += 1
            # INVARIANT: After a successful write, count must be within bounds
            assert simulator.irc_nscount <= simulator.max_nameservers, (
                f"irc_nscount ({simulator.irc_nscount}) exceeded max_nameservers "
                f"({simulator.max_nameservers}) after successful write"
            )
        else:
            rejected_writes += 1
        
        # INVARIANT: Canary values must always be intact (no buffer overflow)
        assert simulator.canaries_intact(), (
            f"Memory corruption detected! Canary values were overwritten. "
            f"irc_nscount={simulator.irc_nscount}, "
            f"ai_addrlen={ai_addrlen}, "
            f"max_ns={max_ns}, element_size={element_size}"
        )
        
        # INVARIANT: irc_nscount must never exceed array capacity
        assert simulator.irc_nscount <= simulator.max_nameservers, (
            f"irc_nscount ({simulator.irc_nscount}) exceeded array capacity "
            f"({simulator.max_nameservers})"
        )
    
    # INVARIANT: Total successful writes must not exceed array capacity
    assert successful_writes <= max_ns, (
        f"More writes succeeded ({successful_writes}) than array capacity ({max_ns})"
    )
    
    # INVARIANT: Final canary check
    assert simulator.canaries_intact(), (
        "Final canary check failed - memory corruption occurred during test"
    )


@pytest.mark.parametrize("count,max_ns", [
    (0, 3),
    (1, 3),
    (2, 3),
    (3, 3),
    (4, 3),   # overflow
    (10, 3),  # severe overflow
    (0, 1),
    (1, 1),
    (2, 1),   # overflow
    (100, 5), # severe overflow
])
def test_nameserver_count_never_exceeds_capacity(count, max_ns):
    """
    Invariant: The number of successfully registered nameservers must never
    exceed the allocated array capacity, regardless of how many are attempted.
    """
    simulator = IRCResolverSimulator(max_nameservers=max_ns, addr_element_size=128)
    valid_addr = socket.inet_aton("8.8.8.8") + b'\x00' * 12  # 16 bytes IPv4
    
    for _ in range(count):
        simulator.safe_add_nameserver(valid_addr, 16)
    
    # INVARIANT: Count must be capped at max_ns
    assert simulator.irc_nscount <= max_ns, (
        f"irc_nscount ({simulator.irc_nscount}) exceeded max_ns ({max_ns})"
    )
    
    # INVARIANT: No memory corruption
    assert simulator.canaries_intact(), "Memory corruption detected"


@pytest.mark.parametrize("ai_addrlen,element_size,should_succeed", [
    (16, 128, True),    # IPv4 addr, fits
    (28, 128, True),    # IPv6 addr, fits
    (128, 128, True),   # Exactly element size, fits
    (129, 128, False),  # One byte over, must reject
    (256, 128, False),  # Double size, must reject
    (65535, 128, False),# Max uint16, must reject
    (0, 128, False),    # Zero length, must reject
    (1, 128, True),     # Minimum valid, fits
    (127, 128, True),   # One under element size, fits
])
def test_addrlen_validation_invariant(ai_addrlen, element_size, should_succeed):
    """
    Invariant: Address data with ai_addrlen exceeding the element size of
    irc_nsaddr_list must always be rejected to prevent buffer overflow.
    """
    simulator = IRCResolverSimulator(max_nameservers=3, addr_element_size=element_size)
    
    # Create addr data of sufficient size
    addr_data = b'\x41' * max(ai_addrlen, 1)
    
    result = simulator.safe_add_nameserver(addr_data, ai_addrlen)
    
    # INVARIANT: Result must match expectation
    assert result == should_succeed, (
        f"Expected {'success' if should_succeed else 'rejection'} for "
        f"ai_addrlen={ai_addrlen}, element_size={element_size}, got {'success' if result else 'rejection'}"
    )
    
    # INVARIANT: No memory corruption regardless of outcome
    assert simulator.canaries_intact(), (
        f"Memory corruption detected with ai_addrlen={ai_addrlen}, element_size={element_size}"
    )
    
    # INVARIANT: If rejected, count must not have increased
    if not should_succeed:
        assert simulator.irc_nscount == 0, (
            f"irc_nscount increased after rejected write (ai_addrlen={ai_addrlen})"
        )