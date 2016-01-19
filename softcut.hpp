#ifndef SPLITTER_SOFTCUT_HPP
#define SPLITTER_SOFTCUT_HPP

#include "cut.hpp"

/*

Softcut Algorithm
 - walk over all node-versions
   - walk over all bboxes
     - if the current node-version is inside the bbox
       - record its id in the bboxes node-tracker

 - initialize the current-way-id to 0
 - walk over all way-versions
   - if current-way-id != 0 and current-way-id != the id of the currently iterated way (in other words: this is a different way)
     - walk over all bboxes
       - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
         - append all nodes of the current-way-nodes set to the extra-node-tracker
     - clear the current-way-nodes set
   - update the current-way-id
   - walk over all way-nodes
     - append the node-ids to the current-way-nodes set
   - walk over all bboxes
     - walk over all way-nodes
       - if the way-node is recorded in the bboxes node-tracker
         - record its id in the bboxes way-id-tracker

- after all ways
  - walk over all bboxes
    - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
      - append all nodes of the current-way-nodes set to the extra-node-tracker

 - walk over all relation-versions
   - walk over all bboxes
     - walk over all relation-members
       - if the relation-member is recorded in the bboxes node- or way-tracker
         - record its id in the bboxes relation-tracker

Second Pass
 - walk over all node-versions
   - walk over all bboxes
     - if the node-id is recorded in the bboxes node-tracker or in the extra-node-tracker
       - send the node to the bboxes writer

 - walk over all way-versions
   - walk over all bboxes
     - if the way-id is recorded in the bboxes way-tracker
       - send the way to the bboxes writer

 - walk over all relation-versions
   - walk over all bboxes
     - if the relation-id is recorded in the bboxes relation-tracker
       - send the relation to the bboxes writer

features:
 - if an object is in the extract, all versions of it are there
 - ways and relations are not changed
 - ways are reference-complete

disadvantages
 - dual pass
 - needs more RAM: 350 MB per BBOX
   - ((1400000000÷8)+(1400000000÷8)+(130000000÷8)+(1500000÷8))÷1024÷1024 MB
 - relations will have dead references

*/


class SoftcutExtractInfo : public ExtractInfo {

public:
    growing_bitset node_tracker;
    growing_bitset extra_node_tracker;
    growing_bitset way_tracker;
    growing_bitset relation_tracker;

    SoftcutExtractInfo(const std::string& name, const osmium::io::File& file, const osmium::io::Header& header) :
        ExtractInfo(name, file, header) {}
};

class SoftcutInfo : public CutInfo<SoftcutExtractInfo> {

public:
    std::multimap<osmium::object_id_type, osmium::object_id_type> cascading_relations_tracker;
};


class SoftcutPassOne : public Cut<SoftcutInfo> {
private:
    osmium::object_id_type current_way_id;
    typedef std::set<osmium::object_id_type> current_way_nodes_t;
    typedef std::set<osmium::object_id_type>::iterator current_way_nodes_it;
    current_way_nodes_t current_way_nodes;

    // - walk over all bboxes
    //   - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
    //     - append all nodes of the current-way-nodes set to the extra-node-tracker
    void write_way_extra_nodes() {
        if(debug) std::cerr << "finished all versions of way " << current_way_id << ", checking for extra nodes" << std::endl;
        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];
            if(extract->way_tracker.get(current_way_id)) {
                if(debug) std::cerr << "way had a node inside extract [" << i << "], recording extra nodes" << std::endl;
                for(current_way_nodes_it it = current_way_nodes.begin(), end = current_way_nodes.end(); it != end; it++) {
                    extract->extra_node_tracker.set(*it);
                    if(debug) std::cerr << "  " << *it;
                }
                if(debug) std::cerr << std::endl;
            }
        }
    }

public:
    SoftcutPassOne(SoftcutInfo *info) : Cut<SoftcutInfo>(info), current_way_id(0), current_way_nodes() {
        std::cerr << "softcut first-pass init" << std::endl;
        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            std::cerr << "\textract[" << i << "] " << info->extracts[i]->name << std::endl;
        }

        if(debug) {
            std::cerr << std::endl << std::endl << "===== NODES =====" << std::endl << std::endl;
        }
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the current node-version is inside the bbox
    //       - record its id in the bboxes node-tracker
    void node(const osmium::Node& node) {
        if(debug) {
            std::cerr << "softcut node " << node.id() << " v" << node.version() << std::endl;
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];
            if(extract->contains(node)) {
                if(debug) std::cerr << "node is in extract [" << i << "], recording in node_tracker" << std::endl;

                extract->node_tracker.set(node.id());
            }
        }
    }

    void after_nodes() {
        if(debug) {
            std::cerr << "after nodes" << std::endl <<
                std::endl << std::endl << "===== WAYS =====" << std::endl << std::endl;
        }
    }

    // - initialize the current-way-id to 0
    // - walk over all way-versions
    //   - if current-way-id != 0 and current-way-id != the id of the currently iterated way (in other words: this is a different way)
    //     - walk over all bboxes
    //       - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
    //         - append all nodes of the current-way-nodes set to the extra-node-tracker
    //     - clear the current-way-nodes set
    //   - update the current-way-id
    //   - walk over all way-nodes
    //     - append the node-ids to the current-way-nodes set
    //   - walk over all bboxes
    //     - walk over all way-nodes
    //       - if the way-node is recorded in the bboxes node-tracker
    //         - record its id in the bboxes way-id-tracker
    //
    // - after all ways
    //   - walk over all bboxes
    //     - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
    //       - append all nodes of the current-way-nodes set to the extra-node-tracker

    void way(const osmium::Way& way) {
        // detect a new way
        if(current_way_id != 0 && current_way_id != way.id()) {
            write_way_extra_nodes();
            current_way_nodes.clear();
        }
        current_way_id = way.id();

        if(debug) {
            std::cerr << "softcut way " << way.id() << " v" << way.version() << std::endl;
        }

        for (const auto& node_ref : way.nodes()) {
            current_way_nodes.insert(node_ref.ref());
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];

            for (const auto& node_ref : way.nodes()) {
                if(extract->node_tracker.get(node_ref.ref())) {
                    if(debug) std::cerr << "way has a node (" << node_ref.ref() << ") inside extract [" << i << "], recording in way_tracker" << std::endl;

                    extract->way_tracker.set(way.id());
                    break;
                }
            }
        }
    }

    void after_ways() {
        write_way_extra_nodes();
        if(debug) {
            std::cerr << "after ways" << std::endl <<
                std::endl << std::endl << "===== RELATIONS =====" << std::endl << std::endl;
        }
    }

    // - walk over all relation-versions
    //   - walk over all bboxes
    //     - walk over all relation-members
    //       - if the relation-member is recorded in the bboxes node- or way-tracker
    //         - record its id in the bboxes relation-tracker
    void relation(const osmium::Relation& relation) {
        if(debug) {
            std::cerr << "softcut relation " << relation.id() << " v" << relation.version() << std::endl;
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            bool hit = false;
            SoftcutExtractInfo *extract = info->extracts[i];

            for (const auto& member : relation.members()) {

                if( !hit && (
                    (member.type() == osmium::item_type::node && extract->node_tracker.get(member.ref())) ||
                    (member.type() == osmium::item_type::way && extract->way_tracker.get(member.ref())) ||
                    (member.type() == osmium::item_type::relation && extract->relation_tracker.get(member.ref()))
                )) {

                    if(debug) std::cerr << "relation has a member (" << member.type() << " " << member.ref() << ") inside extract [" << i << "], recording in relation_tracker" << std::endl;
                    hit = true;

                    extract->relation_tracker.set(relation.id());
                }

                if(member.type() == osmium::item_type::relation) {
                    if(debug) std::cerr << "recording cascading-pair: " << member.ref() << " -> " << relation.id() << std::endl;
                    info->cascading_relations_tracker.insert(std::make_pair(member.ref(), relation.id()));
                }
            }

            if(hit) {
                cascading_relations(extract, relation.id());
            }
        }
    }

    void cascading_relations(SoftcutExtractInfo *extract, osmium::object_id_type id) {
        typedef std::multimap<osmium::object_id_type, osmium::object_id_type>::const_iterator mm_iter;

        std::pair<mm_iter, mm_iter> r = info->cascading_relations_tracker.equal_range(id);
        if(r.first == r.second) {
            return;
        }

        for(mm_iter it = r.first; it !=r.second; ++it) {
            if(debug) std::cerr << "\tcascading: " << it->second << std::endl;

            if(extract->relation_tracker.get(it->second))
                continue;

            extract->relation_tracker.set(it->second);

            cascading_relations(extract, it->second);
        }
    }

    void after_relations() {
        if(debug) {
            std::cerr << "after relations" << std::endl;
        }
    }

    void final() {
        std::cerr << "softcut first-pass finished" << std::endl;
    }
};





class SoftcutPassTwo : public Cut<SoftcutInfo> {

public:
    SoftcutPassTwo(SoftcutInfo *info) : Cut<SoftcutInfo>(info) {
        std::cerr << "softcut second-pass init" << std::endl;

        if(debug) {
            std::cerr << std::endl << std::endl << "===== NODES =====" << std::endl << std::endl;
        }
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the node-id is recorded in the bboxes node-tracker or in the extra-node-tracker
    //       - send the node to the bboxes writer
    void node(const osmium::Node& node) {
        if(debug) {
            std::cerr << "softcut node " << node.id() << " v" << node.version() << std::endl;
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];

            if(extract->node_tracker.get(node.id()) || extract->extra_node_tracker.get(node.id()))
                extract->write(node);
        }
    }

    void after_nodes() {
        if(debug) {
            std::cerr << "after nodes" << std::endl <<
                std::endl << std::endl << "===== WAYS =====" << std::endl << std::endl;
        }
    }

    // - walk over all way-versions
    //   - walk over all bboxes
    //     - if the way-id is recorded in the bboxes way-tracker
    //       - send the way to the bboxes writer
    void way(const osmium::Way& way) {
        if(debug) {
            std::cerr << "softcut way " << way.id() << " v" << way.version() << std::endl;
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];

            if(extract->way_tracker.get(way.id()))
                extract->write(way);
        }
    }

    void after_ways() {
        if(debug) {
            std::cerr << "after ways" << std::endl <<
                std::endl << std::endl << "===== RELATIONS =====" << std::endl << std::endl;
        }
    }

    // - walk over all relation-versions
    //   - walk over all bboxes
    //     - if the relation-id is recorded in the bboxes relation-tracker
    //       - send the relation to the bboxes writer
    void relation(const osmium::Relation& relation) {
        if(debug) {
            std::cerr << "softcut relation " << relation.id() << " v" << relation.version() << std::endl;
        }

        for(int i = 0, l = info->extracts.size(); i<l; i++) {
            SoftcutExtractInfo *extract = info->extracts[i];

            if(extract->relation_tracker.get(relation.id()))
                extract->write(relation);
        }
    }

    void after_relations() {
        if(debug) {
            std::cerr << "after relations" << std::endl;
        }
    }

    void final() {
        std::cerr << "softcut second-pass finished" << std::endl;
    }
};

#endif // SPLITTER_SOFTCUT_HPP

