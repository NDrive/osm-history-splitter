#ifndef SPLITTER_CUT_ALL_BORDERS_HPP
#define SPLITTER_CUT_ALL_BORDERS_HPP

#include "cut.hpp"
#include "growing_bitset.hpp"
#include <map>
#include <tuple>
#include <typeinfo>
/*

Cut_all_borders Algorithm

 - walk over all way-versions
   - walk over all way-nodes
     - Adds the nodes that aren't in node-tracker to a vector
     - if node is in the box hit becames true
   - if hit is true and the vector is not empty (it means their are nodes that belong to a way that has at least one node inside the box - complete ways)
     - Records the id of node in vector to outside-node-tracker

- walk over all relations-versions
  - walk over all relations-nodes
  - Adds the nodes and ways that aren't in node-tracker to a vector
  - if node or way is in the box hit becames true
- if hit is true and the vector is not empty (it means their are nodes or ways that belong to a relation that has at least one node or way inside the box)
  - Records the id of node or way to outside-node-tracker or outside-way-tracker

Second Pass

 - walk over all node-versions
   - walk over all bboxes
     - if the node-id is recorded in the bboxes node-trackers
       - send the way to the bboxes writer

 - walk over all way-versions
   - walk over all bboxes
     - if the way-id is recorded in the bboxes way-trackers
       - send the way to the bboxes writer

 - walk over all relation-versions
   - walk over all bboxes
     - if the relation-id is recorded in the bboxes relation-tracker
       - send the relation to the bboxes writer

features:
 - if an object is in the extract, all versions of it are there
 - ways and relations are not changed
 - ways are reference-complete
 - all the ways and nodes of a relation that has at least one node or way inside the box are added

disadvantages
 - two pass
 - relations will have dead references (other relations)

*/


class Cut_all_bordersExtractInfo : public ExtractInfo {

public:

    growing_bitset node_tracker;	//nodes

    growing_bitset way_tracker;	//ways

    growing_bitset relation_tracker;	//relations


    Cut_all_bordersExtractInfo(const std::string& name, const osmium::io::File& file, const osmium::io::Header& header) :
        ExtractInfo(name, file, header) {}
};

class Cut_all_bordersInfo : public CutInfo<Cut_all_bordersExtractInfo> {

public:
    std::multimap<osmium::object_id_type, osmium::object_id_type> cascading_relations_tracker;

};


class Cut_all_bordersPassOne : public Cut<Cut_all_bordersInfo> {

public:

    Cut_all_bordersPassOne(Cut_all_bordersInfo *info) : Cut<Cut_all_bordersInfo>(info) {
        std::cout << "Start Cut_all_borders:\n";
        for (const auto& extract : info->extracts) {
            std::cout << "\textract " << extract->name << "\n";
        }
        if (debug) {
            std::cerr << "\n\n===cut_all_borders first-pass===\n\n";
        }
    }

    // - walk over all relations-versions
    //   - walk over all relations-nodes
    //     - Adds the nodes and ways that aren't in node-tracker to a vector
    //     - if node or way is in the box hit becames true
    //   - if hit is true and the vector is not empty (it means their are nodes or ways that belong to a relation that has at least one node or way inside the box)
    //     - Records the id of node or way to outside_node_tracker or outside_way_tracker
    void relation(const osmium::Relation& relation) {

    	bool hit = false;

        if (debug) {
            std::cerr << "cut_all_borders relation " << relation.id() << " v" << relation.version() << "\n";
        }

        std::vector<const osmium::RelationMember*> members;
        std::vector<const osmium::TagList*> tags;

        for (auto& tag : relation.tags()) {
            if (strcmp(tag.key(), "boundary") == 0 && strcmp(tag.value(), "administrative") == 0)
                hit = true;
            else if (strcmp(tag.key(), "boundary") == 0 && strcmp(tag.value(), "territorial") == 0)
                hit = true;
            else if (relation.id() == 2186646 || relation.id() == 2559126 || relation.id() == 192797 || relation.id() == 3335661)
                hit = true;
        }

    	for (const auto& extract : info->extracts) {
            if (hit){
                if(!extract->relation_tracker.get(relation.id())){
                    extract->relation_tracker.set(relation.id());
                }
                //Add only the nodes and ways that were not yet in the respective trackers if hit is true
                for (const auto& member : relation.members()) {
                    if (member.type() == osmium::item_type::way && !extract->way_tracker.get(member.ref())){
                            extract->way_tracker.set(member.ref());
                    }
                }
            }
        }
    }
}; // class Cut_all_bordersPassOne


class Cut_all_bordersPassTwo : public Cut<Cut_all_bordersInfo> {

public:

    Cut_all_bordersPassTwo(Cut_all_bordersInfo *info) : Cut<Cut_all_bordersInfo>(info) {
        if (debug) {
            std::cerr << "\n\n===cut_all_borders second-pass===\n\n";
        }
    }

    // - walk over all way-versions
    //   - walk over all bboxes
    //     - if the way-id is recorded in the bboxes way-trackers
    //       - send the way to the bboxes writer
    void way(const osmium::Way& way) {
        if (debug) {
            std::cerr << "cut_all_borders way " << way.id() << " v" << way.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->way_tracker.get(way.id())){
                for (const auto& node_ref : way.nodes()) {
                    if (!extract->node_tracker.get(node_ref.ref())) {
                        extract->node_tracker.set(node_ref.ref());
                    }
                }
            }
        }
    }
}; // class Cut_all_bordersPassTwo


class Cut_all_bordersPassThree : public Cut<Cut_all_bordersInfo> {

public:

    Cut_all_bordersPassThree(Cut_all_bordersInfo *info) : Cut<Cut_all_bordersInfo>(info) {
        if (debug) {
            std::cerr << "\n\n===cut_all_borders thrid-pass===\n\n";
        }
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the node-id is recorded in the bboxes node-trackers
    //       - send the node to the bboxes writer
    void node(const osmium::Node& node) {
        if (debug) {
            std::cerr << "cut_all_borders node " << node.id() << " v" << node.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->node_tracker.get(node.id())){
                extract->write(node);
            }
        }
    }

    // - walk over all way-versions
    //   - walk over all bboxes
    //     - if the way-id is recorded in the bboxes way-trackers
    //       - send the way to the bboxes writer
    void way(const osmium::Way& way) {
        if (debug) {
            std::cerr << "cut_all_borders way " << way.id() << " v" << way.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->way_tracker.get(way.id())){
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
            std::cerr << "cut_all_borders relation " << relation.id() << " v" << relation.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->relation_tracker.get(relation.id())){
                extract->write(relation);
            }
        }
    }
}; // class Cut_all_bordersPassThree

#endif // SPLITTER_CUT_ALL_BORDERS_HPP
