#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    
    if (!seg.header().syn && !_syn) return;
    if (seg.header().syn) {
        _syn = seg.header().syn;
        init_seqno = seg.header().seqno;
    }
    uint64_t abs_seqno = unwrap(seg.header().seqno, init_seqno, last_assembled_seqno);
    last_pushed_seqno = max(last_pushed_seqno, abs_seqno);
    
    if (!seg.header().syn) abs_seqno -= 1;
    _reassembler.push_substring(seg.payload().copy(), abs_seqno, seg.header().fin);
    
    last_assembled_seqno += _reassembler.stream_out().bytes_written() - last_written_bytes;
    last_written_bytes = _reassembler.stream_out().bytes_written();
    
    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn){
        return {};
    } else {
        if (!_reassembler.stream_out().input_ended()) {
            return WrappingInt32(last_assembled_seqno+1) + init_seqno.raw_value();
        } else {
            return WrappingInt32(last_assembled_seqno+2) + init_seqno.raw_value();
        }
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - (_reassembler.stream_out().bytes_written() - _reassembler.stream_out().bytes_read()); }
