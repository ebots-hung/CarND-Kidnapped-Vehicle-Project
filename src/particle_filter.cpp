/**
 * particle_filter.cpp
 *
 * Created on: July 30, 2021
 * Author: Hung Lam
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 75;  // TODO: Set the number of particles

  // Random engine for the sampling
  std::default_random_engine gen;

  // Normal distributions to be sampled
  // Centered in the given initial guess
  // Using the given variance
  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);

  // Initialize particles to the first estimation
  for(int i=0; i<num_particles; i++)
  {
    // Create particle
    // Setting weights to 1
    Particle particle;
    particle.id = i;
    particle.x = dist_x(gen); 
    particle.y = dist_y(gen);
    particle.theta = dist_theta(gen);
    particle.weight = 1.0;

    // Append particle to PF's list
    particles.push_back(particle);

    // Append weight to PF's list
    weights.push_back(particle.weight);
  }

  // Change the filter's state to initialized
  is_initialized = true;

  return;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  // Random engine for the sampling
  std::default_random_engine gen;

  for(int i=0; i<num_particles; i++)
  {
    // Get current particle
    Particle particle = particles[i];

    // Get current pose
    float x = particle.x;
    float y = particle.y;
    float theta = particle.theta;

    // Compute new pose using the Bicycle Motion Model
    float new_x, new_y, new_theta;

    if(fabs(yaw_rate)<0.0001)
    {
      // Use simplified bicycle motion model
      new_x = x + (velocity*cos(theta)*delta_t);
      new_y = y + (velocity*sin(theta)*delta_t);
      new_theta = theta;
    }
    else
    {
      // Apply bycicle model
      new_theta = theta + (yaw_rate*delta_t);
      new_x = x + (velocity/yaw_rate) * (sin(new_theta) - sin(theta));
      new_y = y + (velocity/yaw_rate) * (cos(theta) - cos(new_theta));
    }

    // Normal distributions to be sampled
    std::normal_distribution<double> dist_x(new_x, std_pos[0]);
    std::normal_distribution<double> dist_y(new_y, std_pos[1]);
    std::normal_distribution<double> dist_theta(new_theta, std_pos[2]);

    // Update particle pose
    particles[i].x = dist_x(gen);
    particles[i].y = dist_y(gen);
    particles[i].theta = dist_theta(gen);
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  // Get number of observations
  unsigned int n_observations = observations.size();
  unsigned int n_predicted = predicted.size();

  // For each observation
  for(unsigned int i=0; i<n_observations; i++)
  {
    // Get observation
    float obs_x = observations[i].x;
    float obs_y = observations[i].y;

    // Apply Nearest Neighbour
    // Search the minimum distance between this observation and all the predictions
    float min_dist = std::numeric_limits<float>::max();

    for(unsigned int j=0; j<n_predicted; j++)
    {
      // Get prediction
      float pred_x = predicted[j].x;
      float pred_y = predicted[j].y;

      // Compute distances
      float distance = dist(obs_x, obs_y, pred_x, pred_y);

      // Check if is the best distance so far
      if(distance<min_dist)
      {
        // Update minimum distance and observation ID
        min_dist = distance;
        observations[i].id = predicted[j].id;
      }
    }
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
  // Container for all the weights of the particles (for further normalization)
  float weight_normalizer = 0.0;

  // For each particle
  for(int i=0; i<num_particles; i++)
  {
    // Copy the current particle
    Particle particle = particles[i];

    // Get current particle's properties
    float car_x_pos = particle.x;
    float car_y_pos = particle.y;
    float car_theta = particle.theta;


    // (1) TRANSFORM OBSERVATIONS FROM THE CAR'S COORDINATE SYSTEM TO THE MAP ONE
    // Number of observations
    unsigned int n_obs = observations.size();

    // Vector to store the transformed observations
    std::vector<LandmarkObs> transformed_obs;

    // For each observation
    for(unsigned int j=0; j<n_obs; j++)
    {
      // Get observation in car coordinates
      float obs_x_car = observations[j].x;
      float obs_y_car = observations[j].y;

      // Transform from car coordinates to map ones using the particle's position and orientation
      LandmarkObs transformed_observation;
      transformed_observation.x = obs_x_car*cos(car_theta) - obs_y_car*sin(car_theta) + car_x_pos;
      transformed_observation.y = obs_x_car*sin(car_theta) + obs_y_car*cos(car_theta) + car_y_pos;
      transformed_observation.id = j;

      // Push to the vector of transformed observations
      transformed_obs.push_back(transformed_observation);
    }


    // (2) DATA ASSOCIATION (use the "ParticleFilter::dataAssociation()" function)
    // Number of landmarks in the map
    unsigned int n_landmarks = map_landmarks.landmark_list.size();

    // Create a landmark vector with the map landmarks reached by the range of the sensor
    std::vector<LandmarkObs> predicted;

    // For each landmark in the map
    for(unsigned int j=0; j<n_landmarks; j++)
    {
      Map::single_landmark_s map_landmark = map_landmarks.landmark_list[j];

      // If the landmark is within the sensor range
      if ((fabs((car_x_pos - map_landmark.x_f)) <= sensor_range) && (fabs((car_y_pos - map_landmark.y_f)) <= sensor_range))
        predicted.push_back(LandmarkObs {map_landmark.id_i, map_landmark.x_f, map_landmark.y_f});
    }

    // Associate data (match IDs)
    dataAssociation(predicted, transformed_obs);


    // (3) COMPUTE WEIGHTS BASED ON DISTANCES (Multivariate Gaussian Distribution)
    // Reset particle weight to 1
    particles[i].weight = 1.0;

    // Compute the normalizer
    float sigma_x = std_landmark[0];
    float sigma_y = std_landmark[1];
    float sigma_x_2 = pow(sigma_x, 2);
    float sigma_y_2 = pow(sigma_y, 2);
    float normalizer = 1.0/(2.0*M_PI*sigma_x*sigma_y);

    // Compute non-normalized weight
    // For each tranformed observation
    for(unsigned int j=0; j<n_obs; j++)
    {
      // Get transformed observation
      float trans_obs_x = transformed_obs[j].x;
      float trans_obs_y = transformed_obs[j].y;
      float trans_obs_id = transformed_obs[j].id;

      // Prepare probability
      float multivar_gauss_prob = 1.0;

      // For each predicted landmark
      for(unsigned int k=0; k<predicted.size(); k++)
      {
        // Get predicted landmark
        float pred_landm_x = predicted[k].x;
        float pred_landm_y = predicted[k].y;
        float pred_landm_id = predicted[k].id;

        // Search correspondence
        //// After the data association, the observation and its associated predicted landmark should share ID
        if(trans_obs_id == pred_landm_id)
        {
          // Importance weight - Based on the Multivariate Gaussian Probability Function
          multivar_gauss_prob = normalizer*exp(-((pow(trans_obs_x-pred_landm_x, 2)/sigma_x_2) + (pow(trans_obs_y-pred_landm_y, 2)/sigma_y_2)));
          particles[i].weight *= multivar_gauss_prob;
        }
      }
    }

    // Add non-normalized weight to the normalizer count
    weight_normalizer += particles[i].weight;
  }

  // (4) NORMALIZE WEIGHTS
  for(int i=0; i<num_particles; i++)
  {
    particles[i].weight /= weight_normalizer;
    weights[i] = particles[i].weight;
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  // Create vector of resampled particles
  std::vector<Particle> resampled_particles;

  // Random engine for the sampling
  std::default_random_engine gen;
	
	//Generate random particle index
	std::uniform_int_distribution<int> particle_index(0, num_particles-1);
  int rand_index = particle_index(gen);

  // Helper variables
  float beta = 0.0;
  float double_max_weight = *max_element(weights.begin(), weights.end()) * 2;

  // For each particle
  for(int i=0; i<num_particles; i++)
  {
    std::uniform_real_distribution<float> random_weight(0.0, double_max_weight);
    beta += random_weight(gen);
    
    while(beta>weights[rand_index])
    {
      beta -= weights[rand_index];
      rand_index = (rand_index + 1) % num_particles;
    }

    resampled_particles.push_back(particles[rand_index]);
  }

  particles = resampled_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}
