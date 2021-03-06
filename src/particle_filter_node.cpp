#include <shape-tracking/particle_filter.h>
#include <shape-tracking/constants.h>
#include <bfl/model/systemmodel.h>
#include <bfl/model/measurementmodel.h>
#include <shape-tracking/nonlinearSystemPdf.h>
#include <shape-tracking/nonlinearMeasurementPdf.h>
#include <iostream>
#include <fstream>
// Include file with properties


#include <ros/ros.h>
#include <string>
#include <std_msgs/Empty.h>

#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/PoseStamped.h>



using namespace MatrixWrapper;
using namespace BFL;
using namespace std;

/*
 The necessary SYSTEM MODEL is:
 x_k      = x_{k-1} + v_{k-1} * cos(theta) * delta_t
 y_k      = y_{k-1} + v_{k-1} * sin(theta) * delta_t
*/

class ParticleFilterNode
{
   ros::NodeHandle nh_;
   ros::Subscriber navi_sub;
   ros::Subscriber ranges_sub;
   ros::Publisher pose_pub;
   ros::Publisher particle_pub;

   BFL::NonlinearSystemPdf *sys_pdf;
   BFL::SystemModel<ColumnVector> *sys_model;
   BFL::NonlinearMeasurementPdf *meas_pdf;
   BFL::MeasurementModel<ColumnVector,ColumnVector> *meas_model;
   MCPdf<ColumnVector> *prior_discr;
   CustomParticleFilter *filter;
   ros::Time prevPoseDataTime;
   double dt;
   geometry_msgs::PoseStamped LastPoseDataMsg;

public:
   ParticleFilterNode()
   {
       //navi_sub = nh_.subscribe("/ardrone/navdata", 1, &ParticleFilterNode::InputCb, this);
       ranges_sub = nh_.subscribe("measurements", 1, &ParticleFilterNode::MeasurementCb, this);
       pose_pub = nh_.advertise<geometry_msgs::PoseStamped>("/pose_pf",1);
       particle_pub = nh_.advertise<geometry_msgs::PoseArray>("/particle_cloud",1);
       dt = 0.0;

       sys_model = NULL;
       meas_model = NULL;
       filter = NULL;

       CreateParticleFilter();
   }

   ~ParticleFilterNode()
   {
       delete sys_model;
       delete meas_model;
       delete filter;
   }

   void CreateParticleFilter()
   {
       /****************************
        * NonLinear system model      *
        ***************************/

       // create gaussian
       ColumnVector sys_noise_Mu(STATE_SIZE);
       sys_noise_Mu(1) = MU_SYSTEM_NOISE_X;
       sys_noise_Mu(2) = MU_SYSTEM_NOISE_Y;
       sys_noise_Mu(3) = MU_SYSTEM_NOISE_THETA;

       SymmetricMatrix sys_noise_Cov(STATE_SIZE);
       sys_noise_Cov = 0.0;
       sys_noise_Cov(1,1) = SIGMA_SYSTEM_NOISE_X;
       sys_noise_Cov(1,2) = 0.0;
       sys_noise_Cov(1,3) = 0.0;
       sys_noise_Cov(2,1) = 0.0;
       sys_noise_Cov(2,2) = SIGMA_SYSTEM_NOISE_Y;
       sys_noise_Cov(2,3) = 0.0;
       sys_noise_Cov(3,1) = 0.0;
       sys_noise_Cov(3,2) = 0.0;
       sys_noise_Cov(3,3) = SIGMA_SYSTEM_NOISE_Z;

       Gaussian system_Uncertainty(sys_noise_Mu, sys_noise_Cov);

       // create the nonlinear system model
       //sys_pdf = new NonlinearSystemPdf(system_Uncertainty);
       sys_model = new BFL::SystemModel<ColumnVector> (sys_pdf);

       /*********************************
        * NonLinear Measurement model   *
        ********************************/

       // Construct the measurement noise (a scalar in this case)
       ColumnVector meas_noise_Mu(MEAS_SIZE);
       meas_noise_Mu(1) = MU_MEAS_NOISE;
       meas_noise_Mu(2) = MU_MEAS_NOISE;
       meas_noise_Mu(3) = MU_MEAS_NOISE;
       SymmetricMatrix meas_noise_Cov(MEAS_SIZE);
       meas_noise_Cov(1,1) = SIGMA_MEAS_NOISE;
       meas_noise_Cov(1,2) = 0.0;
       meas_noise_Cov(1,3) = 0.0;
       meas_noise_Cov(2,1) = 0.0;
       meas_noise_Cov(2,2) = SIGMA_MEAS_NOISE;
       meas_noise_Cov(2,3) = 0.0;
       meas_noise_Cov(3,1) = 0.0;
       meas_noise_Cov(3,2) = 0.0;
       meas_noise_Cov(3,3) = SIGMA_MEAS_NOISE;

       Gaussian measurement_Uncertainty(meas_noise_Mu, meas_noise_Cov);

       //meas_pdf = new BFL::NonlinearMeasurementPdf(measurement_Uncertainty);
       //meas_model = new BFL::MeasurementModel<ColumnVector,ColumnVector>(meas_pdf);

       /****************************
        * Linear prior DENSITY     *
        ***************************/
       // Continuous Gaussian prior (for Kalman filters)
       ColumnVector prior_Mu(STATE_SIZE);
       prior_Mu(1) = PRIOR_MU_X;
       prior_Mu(2) = PRIOR_MU_Y;
       prior_Mu(3) = PRIOR_MU_THETA;
       SymmetricMatrix prior_Cov(STATE_SIZE);
       prior_Cov(1,1) = PRIOR_COV_X;
       prior_Cov(1,2) = 0.0;
       prior_Cov(1,3) = 0.0;
       prior_Cov(2,1) = 0.0;
       prior_Cov(2,2) = PRIOR_COV_Y;
       prior_Cov(2,3) = 0.0;
       prior_Cov(3,1) = 0.0;
       prior_Cov(3,2) = 0.0;
       prior_Cov(3,3) = PRIOR_COV_Z;
       Gaussian prior_cont(prior_Mu,prior_Cov);

       // Discrete prior for Particle filter (using the continuous Gaussian prior)
       vector<Sample<ColumnVector> > prior_samples(NUM_SAMPLES);
       prior_discr = new MCPdf<ColumnVector>(NUM_SAMPLES,STATE_SIZE);
       prior_cont.SampleFrom(prior_samples,NUM_SAMPLES,CHOLESKY,NULL);
       prior_discr->ListOfSamplesSet(prior_samples);

       /******************************
        * Construction of the Filter *
        ******************************/
       filter = new CustomParticleFilter (prior_discr, 0.5, NUM_SAMPLES/4.0);
   }

   void MeasurementCb(geometry_msgs::PoseStamped msg)
   {
       ColumnVector measurement(3);
       measurement(1) = msg.pose.position.x/100;
       measurement(2) = msg.pose.position.y/100;
       measurement(3) = msg.pose.position.z/100;
       std::cout << measurement << std::endl;
       {
           filter->Update(meas_model, measurement);
           std::cout << measurement << std::endl;
           PublishParticles();
           PublishPose();
       }
   }

   void InputCb(geometry_msgs::PoseStamped msg)
   {
       if(!prevPoseDataTime.isZero()) dt = (msg.header.stamp - prevPoseDataTime).toSec();
       prevPoseDataTime = msg.header.stamp;
       LastPoseDataMsg = msg;

       ColumnVector input(2);
       //input(1) = msg.vx*dt*0.001;
       //input(2) = msg.vy*dt*0.001;
       //if (LastPoseDataMsg.state==3 || LastPoseDataMsg.state==7 || LastPoseDataMsg.state==4)
           filter->Update(sys_model,input);
   }

   void PublishParticles()
   {
       geometry_msgs::PoseArray particles_msg;
       particles_msg.header.stamp = ros::Time::now();
       particles_msg.header.frame_id = "/map";

       vector<WeightedSample<ColumnVector> >::iterator sample_it;
       vector<WeightedSample<ColumnVector> > samples;

       samples = filter->getNewSamples();

       for(sample_it = samples.begin(); sample_it<samples.end(); sample_it++)
       {
           geometry_msgs::Pose pose;
           ColumnVector sample = (*sample_it).ValueGet();

           pose.position.x = sample(1);
           pose.position.y = sample(2);
           pose.orientation.z = sample(3);

           particles_msg.poses.insert(particles_msg.poses.begin(), pose);
       }
       particle_pub.publish(particles_msg);

   }

   void PublishPose()
   {
       Pdf<ColumnVector> * posterior = filter->PostGet();
       ColumnVector pose = posterior->ExpectedValueGet();
       SymmetricMatrix pose_cov = posterior->CovarianceGet();

       geometry_msgs::PoseStamped pose_msg;
       pose_msg.header.stamp = ros::Time::now();
       pose_msg.header.frame_id = "/map";

       pose_msg.pose.position.x = pose(1);
       pose_msg.pose.position.y = pose(2);
       pose_msg.pose.position.z = pose(3);

       pose_pub.publish(pose_msg);
   }

   void freeMapDependentMemory()
   {
     /*if( map_ != NULL ) {
       map_free( map_ );
       map_ = NULL;
     }*/

     if (sys_model)
       delete sys_model;
     if (meas_model)
       delete meas_model;
     if (filter)
       delete filter;
   }

};


int main(int argc, char** argv)
{
   ros::init(argc, argv, "ParticleFilterNode");
   ParticleFilterNode pfNode;
   ros::spin();
   return 0;
}

