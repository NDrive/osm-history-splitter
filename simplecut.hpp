#ifndef SPLITTER_SIMPLECUT_HPP
#define SPLITTER_SIMPLECUT_HPP

#include "cut.hpp"
#include "growing_bitset.hpp"

/*

Simplecut Algorithm
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


class SimplecutExtractInfo : public ExtractInfo {

public:
    growing_bitset node_tracker;
    growing_bitset way_tracker;
    growing_bitset relation_tracker;

    SimplecutExtractInfo(const std::string& name, const osmium::io::File& file, const osmium::io::Header& header) :
        ExtractInfo(name, file, header) {}
};

class SimplecutInfo : public CutInfo<SimplecutExtractInfo> {

};


class SimplecutPassOne : public Cut<SimplecutInfo> {

    // - walk over all bboxes
    //   - if the way-id is in the bboxes way-id-tracker (in other words: the way is in the output)
    //     - append all nodes of the current-way-nodes set to the extra-node-tracker
   

public:

    SimplecutPassOne(SimplecutInfo *info) : Cut<SimplecutInfo>(info){
        std::cout << "Start Simplecut:\n";
        for (const auto& extract : info->extracts) {
            std::cout << "\textract " << extract->name << "\n";
        }

        std::cout << "\n\n=====simplecut first-pass=====\n\n";
        
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the current node-version is inside the bbox
    //       - record its id in the bboxes node-tracker
    void node(const osmium::Node& node) {
        if (debug) {
            std::cerr << "simplecut node " << node.id() << " v" << node.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->contains(node)) {
                if (debug) std::cerr << "node is in extract, recording in node_tracker\n";

                extract->node_tracker.set(node.id());
            }
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
        if (debug) {
            std::cerr << "simplecut way " << way.id() << " v" << way.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            for (const auto& node_ref : way.nodes()) {
                if (extract->node_tracker.get(node_ref.ref())) {
                    if (debug) {
                        std::cerr << "way has a node (" << node_ref.ref() << ") inside extract, recording in way_tracker\n";
                    }
                    extract->way_tracker.set(way.id());
                    break;
                }
            }
        }
    }

    // - walk over all relation-versions
    //   - walk over all bboxes
    //     - walk over all relation-members
    //       - if the relation-member is recorded in the bboxes node- or way-tracker
    //         - record its id in the bboxes relation-tracker
    void relation(const osmium::Relation& relation) {


        if (debug) {
            std::cerr << "simplecut relation " << relation.id() << " v" << relation.version() << "\n";
        }

        for (const auto& extract : info->extracts) {

            for (const auto& member : relation.members()) {

                if ((member.type() == osmium::item_type::node && extract->node_tracker.get(member.ref())) ||
                    (member.type() == osmium::item_type::way && extract->way_tracker.get(member.ref()))) {

                    if (debug) std::cerr << "relation has a member (" << member.type() << " " << member.ref() << ") inside extract, recording in relation_tracker\n";


                    extract->relation_tracker.set(relation.id());
                    break;
                }     
            }
        }
    }

}; // class SimplecutPassOne


class SimplecutPassTwo : public Cut<SimplecutInfo> {

public:

    SimplecutPassTwo(SimplecutInfo *info) : Cut<SimplecutInfo>(info) {
        if (debug) {
            std::cerr << "simplecut second-pass init\n";
        }
        std::cout << "\n\n=====simplecut second-pass=====\n\n";
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the node-id is recorded in the bboxes node-tracker or in the extra-node-tracker
    //       - send the node to the bboxes writer
    void node(const osmium::Node& node) {
        if (debug) {
            std::cerr << "simplecut node " << node.id() << " v" << node.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->node_tracker.get(node.id())) {
                extract->write(node);
            }
        }
    }

    // - walk over all way-versions
    //   - walk over all bboxes
    //     - if the way-id is recorded in the bboxes way-tracker
    //       - send the way to the bboxes writer
    void way(const osmium::Way& way) {
        if (debug) {
            std::cerr << "simplecut way " << way.id() << " v" << way.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->way_tracker.get(way.id())) {
                extract->write(way);
            }
        }
    }

    // - walk over all relation-versions
    //   - walk over all bboxes
    //     - if the relation-id is recorded in the bboxes relation-tracker
    //       - send the relation to the bboxes writer
    void relation(const osmium::Relation& relation) {
        if (debug) {
            std::cerr << "simplecut relation " << relation.id() << " v" << relation.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->relation_tracker.get(relation.id())) {
                extract->write(relation);
            }
        }
    }

}; // class SimplecutPassTwo

#endif // SPLITTER_SIMPLECUT_HPP

