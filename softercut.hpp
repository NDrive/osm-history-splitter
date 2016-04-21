#ifndef SPLITTER_SOFTERCUT_HPP
#define SPLITTER_SOFTERCUT_HPP

#include "cut.hpp"
#include "growing_bitset.hpp"
#include <map>
#include <tuple>
#include <typeinfo>
/*

Softercut Algorithm
 - walk over all node-versions
   - walk over all bboxes
     - if the current node-version is inside the bbox
       - record its id in the bboxes inside-node-tracker

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
 - walk over all way-versions
   - walk over all bboxes
     - if the way-id is recorded in the bboxes outside-way-tracker
       - send the all the nodes of the way to outside-node-tracker

Third Pass

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
 - three pass
 - more memory than Softcut algoritm
 - relations will have dead references (other relations)

*/


class SoftercutExtractInfo : public ExtractInfo {

public:
						
    growing_bitset inside_node_tracker;	//nodes inside the box	
    growing_bitset outside_node_tracker; //nodes outside the box	

    growing_bitset inside_way_tracker;	//ways inside the box	
    growing_bitset outside_way_tracker;	//ways outside the box	

    growing_bitset relation_tracker;	//relations    


    SoftercutExtractInfo(const std::string& name, const osmium::io::File& file, const osmium::io::Header& header) :
        ExtractInfo(name, file, header) {}
};

class SoftercutInfo : public CutInfo<SoftercutExtractInfo> {

public: 
    std::multimap<osmium::object_id_type, osmium::object_id_type> cascading_relations_tracker;

};


class SoftercutPassOne : public Cut<SoftercutInfo> {

public:

    SoftercutPassOne(SoftercutInfo *info) : Cut<SoftercutInfo>(info) {
        std::cout << "Start Softercut:\n";
        for (const auto& extract : info->extracts) {
            std::cout << "\textract " << extract->name << "\n";
        }
        std::cout << "\n\n===softercut first-pass===\n\n";
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the current node-version is inside the bbox
    //       - record its id in the bboxes inside_node_tracker
    void node(const osmium::Node& node) {
        if (debug) {
            std::cerr << "softercut node " << node.id() << " v" << node.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->contains(node)) {
                if (debug) 
                    std::cerr << "node is in extract, recording in node_tracker\n";
                if(!extract->inside_node_tracker.get(node.id())){
                    extract->inside_node_tracker.set(node.id());
                }
            }
        }
    }

    // - walk over all way-versions
    //   - walk over all way-nodes
    //     - Adds the nodes that aren't in node-tracker to a vector
    //     - if node is in the box hit becames true
    //   - if hit is true and the vector is not empty (it means their are nodes that belong to a way that has at least one node inside the box - complete ways)
    //       - Records the id of node to outside_node_tracker
    void way(const osmium::Way& way) {
        // detect a new way
        bool hit = false;


        if (debug) {
            std::cerr << "softercut way " << way.id() << " v" << way.version() << "\n";
        }

        std::set<osmium::object_id_type> way_nodes;

        for (const auto& extract : info->extracts) {
            way_nodes.clear();
            hit = false;
            for (const auto& node_ref : way.nodes()) {
                if (extract->inside_node_tracker.get(node_ref.ref())) {
                    hit = true;
                    if (debug) {
                        std::cerr << "way has a node (" << node_ref.ref() << ") inside extract, recording in way_tracker\n";
                    }
                }
                else{
                   way_nodes.insert(node_ref.ref()); 
                }
            }
            if (hit){
                if(!extract->inside_way_tracker.get(way.id())){
                   extract->inside_way_tracker.set(way.id());
                }
                //Add only the nodes that were not yet in the the node-tracker if hit is true
                if (!way_nodes.empty()){
                    for (const auto id : way_nodes) {
                        if(!extract->outside_node_tracker.get(id)){
                            extract->outside_node_tracker.set(id);
                        }
                    }
                }
            }
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
            std::cerr << "softercut relation " << relation.id() << " v" << relation.version() << "\n";
        }
    	
        std::vector<const osmium::RelationMember*> members;
        members.reserve(relation.members().size());
    	
    	for (const auto& extract : info->extracts) {
    		members.clear();
                hit = false;
    		for (const auto& member : relation.members()) {
    			
            		if ((member.type() == osmium::item_type::node && extract->inside_node_tracker.get(member.ref())) ||
                       	(member.type() == osmium::item_type::way  && extract->inside_way_tracker.get(member.ref()))) {
                       	 	hit = true;
    			}
    			else if ((member.type() == osmium::item_type::node && !extract->inside_node_tracker.get(member.ref())) ||
                       	(member.type() == osmium::item_type::way  && !extract->inside_way_tracker.get(member.ref()))) {
    				      members.push_back(&member); 
                    }
    		}
                if (hit){
                     if(!extract->relation_tracker.get(relation.id())){
    			extract->relation_tracker.set(relation.id());
                     }
    		     //Add only the nodes and ways that were not yet in the respective trackers if hit is true
    		     if (!members.empty()){
    			for (auto memptr : members) {
    				if (memptr->type() == osmium::item_type::node && !extract->outside_node_tracker.get(memptr->ref())){
    						extract->outside_node_tracker.set(memptr->ref());
    	 			}
    				
    				else if (memptr->type() == osmium::item_type::way && !extract->outside_way_tracker.get(memptr->ref())){
    						extract->outside_way_tracker.set(memptr->ref());
    				}
    			}
              	    }
               }
        }
    }
}; // class SoftercutPassOne


class SoftercutPassTwo : public Cut<SoftercutInfo> {

public:

    SoftercutPassTwo(SoftercutInfo *info) : Cut<SoftercutInfo>(info) {
        if (debug) {
            std::cerr << "softercut second-pass init\n";
        }
        std::cout << "\n\n===softercut second-pass===\n\n";
    }

    // - walk over all way-versions
    //   - walk over all bboxes
    //     - if the way-id is recorded in the outside_way_tracker and node-id of the way is not in outside_node_tracker
    //       - send the node-id node tracker
    void way(const osmium::Way& way) {
        if (debug) {
            std::cerr << "softercut way " << way.id() << " v" << way.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->outside_way_tracker.get(way.id())) {
                for (const auto& node_ref : way.nodes()) {
                    if (!extract->outside_node_tracker.get(node_ref.ref())) {
                        extract->outside_node_tracker.set(node_ref.ref());
                    }            
                }
           }
    	}
    }
}; // class SoftercutPassTwo

class SoftercutPassThree : public Cut<SoftercutInfo> {

public:

     SoftercutPassThree(SoftercutInfo *info) : Cut<SoftercutInfo>(info) {
        if (debug) {
            std::cerr << "softercut third-pass init\n";
        }
        std::cout << "\n\n===softercut third-pass===\n\n";
    }

    // - walk over all node-versions
    //   - walk over all bboxes
    //     - if the node-id is recorded in the bboxes node-trackers
    //       - send the node to the bboxes writer
    void node(const osmium::Node& node) {
        if (debug) {
            std::cerr << "softercut node " << node.id() << " v" << node.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
            if (extract->inside_node_tracker.get(node.id())){
                extract->write(node);
            }
            else if (extract->outside_node_tracker.get(node.id())) {
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
            std::cerr << "softercut way " << way.id() << " v" << way.version() << "\n";
        }
        for (const auto& extract : info->extracts) {
	    	if (extract->inside_way_tracker.get(way.id())){
                extract->write(way);
            }
            else if (extract->outside_way_tracker.get(way.id())) {
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
            std::cerr << "softercut relation " << relation.id() << " v" << relation.version() << "\n";
        }

        for (const auto& extract : info->extracts) {
            if (extract->relation_tracker.get(relation.id())){
                extract->write(relation);
            }
        }
    }
}; // class SoftercutPassThree

#endif // SPLITTER_SOFTERCUT_HPP

