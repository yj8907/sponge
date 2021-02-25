#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}, _rto(retx_timeout), _stream(capacity),  _rt_timer{} {
        _curr_ack = WrappingInt32(_isn.raw_value());
    }

uint64_t TCPSender::bytes_in_flight() const {
    return _next_seqno - unwrap(_curr_ack, _isn, _next_seqno); }

void TCPSender::fill_window() {
    // refuse to send more bytes till acked
    if (_relative_ws < 1) return;
    
    size_t bytes_to_read;
    while (!_fin && _relative_ws > 0) {
        if (_stream.buffer_empty() && _next_seqno > 0 && !_stream.input_ended())  break;
        
        TCPHeader header{};
        header.syn = _next_seqno == 0;
        bytes_to_read = min(_relative_ws, TCPConfig::MAX_PAYLOAD_SIZE);
                
        string payload = _stream.read(bytes_to_read);
        
        header.seqno = wrap(_next_seqno, _isn);
        header.fin = _stream.eof();
        _fin = _stream.eof();
            
        Buffer buffer(move(payload));
        TCPSegment segment;
        segment.header() = header;
        segment.payload() = buffer;
                
        if ( _relative_ws < segment.length_in_sequence_space()){
            segment.header().fin = false; _fin= false;
        }
        
        _segments_out.push(segment);
        _last_segments_out.push(segment);
        _next_seqno += segment.length_in_sequence_space();
        _relative_ws -= segment.length_in_sequence_space();
        
    }
    if (!_segments_out.empty() && !_rt_timer.started()) _rt_timer.start(_curr_time);
}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    
    _window_size = window_size;
    _relative_ws = unwrap(ackno, _isn, _next_seqno) + window_size >= _next_seqno ?
        unwrap(ackno, _isn, _next_seqno) + window_size - _next_seqno : 0;
    if (window_size == 0){
        _relative_ws = _relative_ws > 0 ? _relative_ws : 1;
    }
    
    if ( unwrap(ackno, _isn, _next_seqno) > unwrap(_curr_ack, _isn, _next_seqno) ){
        // reset timer
        _rto = _initial_retransmission_timeout;
        _consecutive_rt = 0;
        
        _curr_ack = ackno;
        uint64_t unwrapped_ackno = unwrap(ackno, _isn, _next_seqno);
        while (!_last_segments_out.empty()){
            uint64_t unwrapped_seqno = unwrap(_last_segments_out.front().header().seqno,
                                              _isn, _next_seqno);
            if (unwrapped_seqno + _last_segments_out.front().length_in_sequence_space() <= unwrapped_ackno ) {
                _last_segments_out.pop();
            } else {
                break;
            }
        }
        if (_last_segments_out.empty()) {
            _rt_timer.stop();
        } else {
            _rt_timer.restart(_curr_time);
        }
                
    }
    fill_window();
    
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _curr_time += ms_since_last_tick;
    if (_rt_timer.alarm(_curr_time, _rto)) retransmit();
}

void TCPSender::retransmit() {
    if (!_last_segments_out.empty()){
        _segments_out.push(_last_segments_out.front());
        if (_window_size > 0 ) _rto *= 2;
        _consecutive_rt += 1;
        _rt_timer.restart(_curr_time);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_rt; }

void TCPSender::send_empty_segment() {
    TCPHeader header{};
    header.seqno = wrap(_next_seqno, _isn);

    TCPSegment segment{};
    segment.header() = header;
    _segments_out.push(segment);
}

TCPTimer::TCPTimer() {}

void TCPTimer::start(size_t curr_time){
    _running = true;
    _init_time = curr_time;
}

void TCPTimer::stop(){
    _running = false;
}

void TCPTimer::restart(size_t curr_time){
    stop();
    start(curr_time);
}

bool TCPTimer::alarm(size_t curr_time, size_t rto){
    return _running && (curr_time - _init_time >= rto);
}

bool TCPTimer::started(){
    return _running;
}

