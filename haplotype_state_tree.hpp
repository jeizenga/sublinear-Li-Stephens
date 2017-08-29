#ifndef HAPLOTYPE_STATE_TREE_H
#define HAPLOTYPE_STATE_TREE_H

#include "lh_probability.hpp"
#include "lh_delay_multiplier.hpp"
#include "haplotype_state_node.hpp"
#include <unordered_set>

using namespace std;

struct haplotypeStateTree{
private:
  const linearReferenceStructure* reference;
  const penaltySet* penalties;
  const haplotypeCohort* cohort;
  vector<size_t> segregating_sites;
  size_t initial_position = SIZE_MAX;
public:
  haplotypeStateTree();
  haplotypeStateTree(const linearReferenceStructure* reference, const penaltySet* penalties, 
              const haplotypeCohort* cohort);
  ~haplotypeStateTree();
  
  haplotypeStateNode* root;
  
  void start_with_active_site(size_t i);
  void start_with_inactive_site(size_t i, alleleValue a);
  void start_with_span(size_t length);
  void set_initial_position(size_t position);
  
  // do not call on the first site instead of start_with_span(). It will not
  // correctly initialize, nor will it handle the case that the initial span is
  // truncated relative to the reference
  void fill_in_span_before(haplotypeStateNode* n, size_t i);
  void extend_node_by_allele_at_site(haplotypeStateNode* n, 
          size_t i, alleleValue a);
  void branch_node_by_alleles_at_site(haplotypeStateNode* n, 
          size_t i, double threshold = 0);
  
  haplotypeStateNode* alleles_to_state(const vector<alleleValue>& identifiers) const;
  vector<alleleValue> state_to_alleles(const haplotypeStateNode* state_node) const;
  
  void remove_node(haplotypeStateNode* n);
  void remove_node_and_unshared_ancestors(haplotypeStateNode* n);
  
  void remove_unlikely_children(haplotypeStateNode* n, double threshold);
  
  size_t length_to_node(haplotypeStateNode* n) const;
};

#endif