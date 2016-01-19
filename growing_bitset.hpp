#ifndef GROWING_BITSET_HPP
#define GROWING_BITSET_HPP

#include <iostream>
#include <vector>
#include <memory>

#include <osmium/osm/types.hpp>

class growing_bitset
{
private:
    typedef std::vector<bool> bitvec_t;
    typedef bitvec_t* bitvec_ptr_t;
    typedef std::vector< bitvec_ptr_t > bitmap_t;


    static const size_t segment_size = 50*1024*1024;
    bitmap_t bitmap;

    bitvec_ptr_t find_segment (size_t segment) {
        if (segment >= bitmap.size()) {
            bitmap.resize(segment+1);
        }

        bitvec_ptr_t ptr = bitmap.at(segment);
        if(!ptr) {
            bitmap[segment] = ptr = new std::vector<bool>(segment_size);
        }

        return ptr;
    }

    bitvec_ptr_t find_segment (size_t segment) const {
        if (segment >= bitmap.size()) {
            return nullptr;
        }

        return bitmap.at(segment);
    }

public:
    ~growing_bitset() {
        for (bitmap_t::iterator it=bitmap.begin(), end=bitmap.end(); it != end; it++) {
            bitvec_ptr_t ptr = (*it);
            if(ptr) delete ptr;
        }
    }

    void set(const osmium::object_id_type pos) {
        size_t
            segment = static_cast<osmium::object_id_type>(pos) / static_cast<osmium::object_id_type>(segment_size),
            segmented_pos = static_cast<osmium::object_id_type>(pos) % static_cast<osmium::object_id_type>(segment_size);

        bitvec_ptr_t bitvec = find_segment(segment);
        bitvec->at(segmented_pos) = true;
    }

    bool get(const osmium::object_id_type pos) const {
        size_t
            segment = static_cast<osmium::object_id_type>(pos) / static_cast<osmium::object_id_type>(segment_size),
            segmented_pos = static_cast<osmium::object_id_type>(pos) % static_cast<osmium::object_id_type>(segment_size);

        bitvec_ptr_t bitvec = find_segment(segment);
        if(!bitvec) return false;
        return static_cast<bool>(bitvec->at(segmented_pos));
    }

    void clear() {
        for (bitmap_t::iterator it=bitmap.begin(), end=bitmap.end(); it != end; it++) {
            bitvec_ptr_t ptr = (*it);
            ptr->clear();
        }
    }
};

#endif
