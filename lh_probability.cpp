#include <cmath>
#include "lh_probability.hpp"


penaltySet::~penaltySet() {
  
}

penaltySet::penaltySet(double rho, double mu, int H) : H(H), 
          rho(rho), mu(mu) {
  log_H = log(H);
  one_minus_rho = log1p(-exp(rho));
  one_minus_mu = log1p(-exp(mu));
  one_minus_2mu = log1p(-2*exp(mu));
  ft_of_one = log1p(-2*exp(rho));
  fs_of_one = logsum(ft_of_one, rho + log_H);
}

DPUpdateMap penaltySet::get_current_map(double last_sum, bool match_is_rare) {
  double p_recomb_x_S = rho + last_sum;
  if(match_is_rare) {
    return DPUpdateMap(mu + ft_of_one, p_recomb_x_S - ft_of_one);
  } else {
    return DPUpdateMap(one_minus_mu + ft_of_one, p_recomb_x_S - ft_of_one);
  }
}

double penaltySet::get_minority_map_correction(bool match_is_rare) {
  if(match_is_rare) {
    return one_minus_mu - mu;
  } else {
    return mu - one_minus_mu;
  }
}

void penaltySet::update_S(double& S, vector<double>& summands, 
              bool match_is_rare) {
  if(match_is_rare) {
    double correct_to_1_m_2mu = one_minus_2mu - one_minus_mu;
    S += fs_of_one + mu;
    S = logsum(S, correct_to_1_m_2mu + log_big_sum(summands));
  } else {
    double correct_to_1_m_2mu = one_minus_2mu - mu;
    S += fs_of_one + mu;
    S = logdiff(S, correct_to_1_m_2mu + log_big_sum(summands));
  }
}

haplotypeMatrix::haplotypeMatrix(linearReferenceStructure* ref, penaltySet* pen,
          haplotypeCohort* cohort) :
          reference(ref), cohort(cohort), penalties(pen),
          map(delayMap(cohort->size(), 0)) {
  S = 0;
  R = vector<double>(cohort->size(),0);
}

haplotypeMatrix::haplotypeMatrix(const haplotypeMatrix &other, 
            bool copy_map = true) {
	reference = other.reference;
	cohort = other.cohort;
	penalties = other.penalties;
	last_extended = other.last_extended;
	last_span_extended = other.last_span_extended;
  last_site_extended = other.last_site_extended;
	last_allele = other.last_allele;
	S = other.S;
	R = other.R;
	if(copy_map) {
		map = delayMap(other.map);
	} else {
		map = delayMap(cohort->size(), last_extended);
	}
}

haplotypeMatrix::~haplotypeMatrix() {
  
}

void haplotypeMatrix::record_last_extended(alleleValue a) {
  last_extended++;
  last_site_extended++;
  last_allele = a;
}

bool haplotypeMatrix::last_extended_is_span() {
  return (last_extended == last_span_extended);
}

delayMap haplotypeMatrix::get_map() {
  return map;
}

void haplotypeMatrix::initialize_probability(inputHaplotype* q) {
  if(q->has_sites()) {
    if(q->has_left_tail()) {
      initialize_probability(q->get_site_index(0), q->get_allele(0),
                q->get_left_tail(), q->get_augmentations(0));
    } else {
      initialize_probability(q->get_site_index(0), q->get_allele(0));
    }
  } else {
    initialize_probability_at_span(q->get_left_tail(), 
              q->get_augmentations(-1));
  }
}

void haplotypeMatrix::extend_probability_at_site(inputHaplotype* q, size_t j) {
  extend_probability_at_site(q->get_site_index(j), q->get_allele(j));
}

void haplotypeMatrix::extend_probability_at_span_after(inputHaplotype* q, 
            size_t j) {
  extend_probability_at_span_after(q->get_site_index(j), 
            q->get_augmentations(j));
}

double haplotypeMatrix::calculate_probability(inputHaplotype* q) {
  initialize_probability(q);
  for(size_t j = 1; j < q->number_of_sites(); j++) {
    extend_probability_at_site(q, j);
    if(q->has_span_after(j)) {
      extend_probability_at_span_after(q, j);
    }
  }
  return S;
}

void haplotypeMatrix::initialize_probability(size_t site_index, alleleValue a,
            size_t left_tail_length, size_t augmentation_count) {
  if(left_tail_length != 0) {
    initialize_probability_at_span(left_tail_length, augmentation_count);
    extend_probability_at_site(site_index, a);
  } else {
    initialize_probability_at_site(site_index, a);
  }
}

void haplotypeMatrix::initialize_probability_at_span(size_t length, 
              size_t augmentation_count) {
  // There is a uniform 1/|H| probability of starting on any given haplotype.
  // All emission probabilities are the same. So all R-values are the same
  double fs_of_l = (length - 1) * penalties->fs_of_one;
  double mutation_factor =
            (length - augmentation_count) * (penalties->one_minus_mu) +
            augmentation_count * (penalties->mu);
  double common_initial_R = mutation_factor + fs_of_l - penalties->log_H;
  for(size_t i = 0; i < R.size(); i++) {
    R[i] = common_initial_R;
  }
  S = mutation_factor + fs_of_l;
  
  last_span_extended = -1;
}

void haplotypeMatrix::initialize_probability_at_site(size_t site_index, 
            alleleValue a) {
  vector<size_t> matches = cohort->get_matches(site_index, a);
  vector<size_t> non_matches = 
            cohort->get_non_matches(site_index, a);
  // There are only two possible R-values at this site. There is a uniform
  // 1/|H| probability of starting on any given haplotype; the emission
  // probabilities account for differences in R-value
  double match_initial_value = -penalties->log_H + penalties->one_minus_mu;
  double nonmatch_initial_value = -penalties->log_H + penalties->mu;

  // Set the ones which match the query haplotype
  for(size_t i = 0; i < matches.size(); i++) {
    R[matches[i]] = match_initial_value;
  }
  // And the ones which don't
  for(size_t i = 0; i < non_matches.size(); i++) {
    R[non_matches[i]] = nonmatch_initial_value;
  }

  S = -penalties->log_H + 
              logsum(log(matches.size()) + penalties->one_minus_mu,
                     log(non_matches.size()) + penalties->mu);
  record_last_extended(a);
}

void haplotypeMatrix::extend_probability_at_site(size_t site_index,
            alleleValue a) {
  bool match_is_rare = cohort->match_is_rare(site_index, a);
  DPUpdateMap current_map = penalties->get_current_map(S, match_is_rare);
  map.add_map_for_site(current_map);
  double p_match = penalties->one_minus_mu;
  double p_nonmatch = penalties->mu;
  if(cohort->number_matching(site_index, a) == 0) {
    // separate case to avoid log-summing "log 0"
    S = p_nonmatch + S + penalties->fs_of_one;
  } else if(cohort->number_not_matching(site_index, a) == 0) {
    // separate case to avoid log-summing "log 0"
    S = p_match + S + penalties->fs_of_one;
  } else {
    vector<size_t> active_rows = cohort->get_active_rows(site_index, a);
    map.update_map_with_active_rows(active_rows);
    double correction = penalties->get_minority_map_correction(match_is_rare);
    
    for(size_t i = 0; i < active_rows.size(); i++) {
      R[active_rows[i]] = correction + 
                  calculate_R(R[active_rows[i]], map.get_map(active_rows[i]));  
    }
    map.reset_rows(active_rows);
    vector<double> summands;
    for(size_t i = 0; i < active_rows.size(); i++) {
      summands.push_back(R[active_rows[i]]);
    }
    penalties->update_S(S, summands, match_is_rare);
  }
  record_last_extended(a);
  return;
}

void haplotypeMatrix::extend_probability_at_span_after(size_t site_index,
            size_t augmentation_count = 0) {
  size_t length = reference->span_length_after(site_index);
  double lfsl = length * penalties->fs_of_one;
  double lftl = length * penalties->ft_of_one;
  double mut_pen = (length - augmentation_count) * penalties->one_minus_mu +
              augmentation_count * penalties->mu;
  double RHS = S - penalties->log_H + logdiff(lfsl, lftl);
  map.update_map_with_span(mut_pen + lftl, RHS - lftl);
  S = mut_pen + S + lfsl;
  last_span_extended = last_extended + 1;
}

void haplotypeMatrix::take_snapshot() {
  if(last_extended > 0) {
    // step forward all delayMap slots which are not up to date
    map.hard_update_all();
    size_t j = last_extended;
    bool reference_is_homogenous = 
              (cohort->number_matching(j, last_allele) == 0 ||
              cohort->number_not_matching(j, last_allele) == 0);
    if(reference_is_homogenous || last_extended_is_span()) {
      for(size_t i = 0; i < cohort->size(); i++) {
        R[i] = calculate_R(R[i], map.get_map(i));
      }
    } else if(cohort->match_is_rare(j, last_allele)) {
      // we have already calculated R at all matching rows
      vector<size_t> non_matches = cohort->get_non_matches(j, last_allele);
      for(size_t i = 0; i < non_matches.size(); i++) {
        R[non_matches[i]] =
                  calculate_R(R[non_matches[i]], map.get_map(non_matches[i]));
      }
    } else {
      // we have already calculated R at all non-matching rows
      vector<size_t> matches = cohort->get_matches(j, last_allele);
      for(size_t i = 0; i < matches.size(); i++) {
        R[matches[i]] =
                  calculate_R(R[matches[i]], map.get_map(matches[i]));
      }
    }
    // since all R-values are up to date, we do not need entries in the delayMap
    // therefore we can clear them all and replace them with the identity map
    map.hard_clear_all();
  }
} 

double haplotypeMatrix::prefix_likelihood() {
  return S;
}

double haplotypeMatrix::partial_likelihood_by_row(size_t row) {
  return R[row];
}

double calculate_R(double oldR, DPUpdateMap map) {
  return map.evaluate_at(oldR);
}

double calculate_R(double oldR, double coefficient, double constant) {
  return calculate_R(oldR, DPUpdateMap(coefficient, constant));
}