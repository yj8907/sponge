#include "stream_reassembler.hh"
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (eof) {
        stream_size = index + data.size();
        _eof = eof;
    }
    
    add_substring_to_map(data, index);
    push_substring_to_stream();
    remove_substring();
    
    if (_eof && nextIndex == stream_size) _output.end_input();
    
    return;
}

void StreamReassembler::add_substring_to_map(const std::string &data, const uint64_t index){
    
    auto ptr = _store.lower_bound(index);
    
    if (ptr != _store.end() && ptr->first == index){
        if (data.size() > ptr->second.size()) {
            _store_size += data.size() - ptr->second.size();
            ptr->second = data;
        }
    } else {
        ptr = _store.insert(ptr, pair<uint64_t, string>(index, data));
        _store_size += data.size();
    }
    
    if (ptr != _store.begin()) --ptr;
    merge_substring(ptr);
    
    return;
}

void StreamReassembler::merge_substring(map<uint64_t, string>::iterator iter){
    
    uint64_t curr_index = iter->first;
    string &curr_string = iter->second;
    
    ++iter;
    if (iter == _store.end()) return;
    if (curr_index + curr_string.size() <= iter->first) return;
    
    size_t start_pos;
    if (curr_index + curr_string.size() > iter->first) {
        start_pos = iter->first - curr_index;
        if (curr_index + curr_string.size() > iter->first + iter->second.size()){
            _store_size += curr_string.size() - start_pos - iter->second.size();
            iter->second = curr_string.substr(start_pos);
        }
        _store_size -= curr_string.size() - start_pos;
        curr_string = curr_string.substr(0, start_pos);
    }
    
    merge_substring(iter);
        
}

void StreamReassembler::push_substring_to_stream(){
    
    auto iter = _store.begin();
    
    size_t push_size = 0;
    size_t str_push_pos = 0;
    
    while (_output.remaining_capacity() > 0
           && iter != _store.end() && nextIndex >= iter->first ){
        
        if (nextIndex >= iter->first + iter->second.size()) {
            ++iter; continue;
        }
        
        str_push_pos = nextIndex - iter->first;
        push_size = min(iter->second.size() - str_push_pos, _output.remaining_capacity());
        
        _output.write(iter->second.substr(str_push_pos, push_size));
        nextIndex += push_size;

    }
                
    if (iter != _store.begin()) {
        for (auto iter_count = _store.begin(); iter_count != iter; ++iter_count) {
            _store_size -= iter_count->second.size();
        }
        _store.erase(_store.begin(), iter);
    }
    
    return;
}

void StreamReassembler::remove_substring(){
    
    size_t bytes_to_delete = 0;
    auto iter = _store.end();
    while (iter != _store.begin()  &&
           _output.buffer_size() + unassembled_bytes() - bytes_to_delete > _capacity){
        --iter;
        bytes_to_delete += iter->second.size();
    }
    _store.erase(iter, _store.end());
    
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _store_size; }

bool StreamReassembler::empty() const { return _store_size == 0; }
