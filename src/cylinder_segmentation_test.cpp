﻿#include "cylinder_segmentation_hough.h"
#include <ctime>
#include <tf/transform_broadcaster.h>



int main (int argc, char** argv)
{
	ros::init(argc, argv, "cylinder_publisher");

	/**
	* NodeHandle is the main access point to communications with the ROS system.
	* The first NodeHandle constructed will fully initialize this node, and the last
	* NodeHandle destructed will close down the node.
	*/

	ros::NodeHandle n;
	ros::Rate loop_rate(30);
	unsigned int angle_bins=50;
	unsigned int radius_bins=100;
 	unsigned int position_bins=50;

	// TESTING PARAMS
 	float mix_radius=10.0;
 	float max_radius=1000.0;
	float noise_level=0.1; // percentage of object size (std_dev)
	ros::Publisher cloud_pub;
	cloud_pub=n.advertise<pcl::PointCloud<PointT> >( "cylinders_pcl", 0 );

	while (ros::ok())
	{

		pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
		// Read in the cloud data
		//pcl::PCDReader reader;
		//reader.read (argv[1], *cloud);
		cloud->header.frame_id="world";

		int height_samples=30;
		int angle_samples=30;

		// Generate random radius
		cv::Mat radius(1, 1, CV_32F);
		do {

			cv::randn(radius, 0.1, 0.6);
		} while(radius.at<float>(0,0)>0.3||radius.at<float>(0,0)<0.05);

		// Generate random height
		cv::Mat height(1, 1, CV_32F);
		cv::randn(height, 0.3, 0.6);

		// Generate cylinder according to parameters
		float angle_step=2.0*M_PI/angle_samples;
		float height_step=fabs(height.at<float>(0,0))/height_samples;

		//std::cout << "height:" << height.at<float>(0,0) << " radius:" << radius.at<float>(0,0) << std::endl;

	
		for(int a=0; a < angle_samples; ++a)
		{

			float x,y,z;
			x=cos(angle_step*a)*fabs(radius.at<float>(0,0));
			y=sin(angle_step*a)*fabs(radius.at<float>(0,0));
			for(int h=0; h < height_samples; ++h)
			{
				z=(float)height_step*h;
				pcl::PointXYZ point(x,y,z);
				cloud->push_back(point);
			}
		}
		//std::cerr << "PointCloud has: " << cloud->points.size () << " data points." << std::endl;


		Eigen::Vector4f min_pt,max_pt;
		pcl::getMinMax3D(*cloud,min_pt,max_pt);


		Eigen::Vector3f rot_dir;
		cv::Mat aux(1, 1, CV_32F);

		// Generate random orientation
		cv::randn(aux, 0.0, 1.0);
		rot_dir(0,0)=aux.at<float>(0,0);

		cv::randn(aux, 0.0, 1.0);
		rot_dir(1,0)=aux.at<float>(0,0);

		cv::randn(aux, 0.0, 1.0);
		rot_dir(2,0)=aux.at<float>(0,0);

		rot_dir.normalize();

		cv::Mat angle(1, 1, CV_32F);
		cv::randn(angle, 0.0, 1.0);
		//Eigen::Vector3f rot_dir(1.0,0.0,0.0);
		rot_dir.normalize();
		Eigen::Matrix3f rot;
		rot=Eigen::AngleAxisf(M_PI*angle.at<float>(0,0),rot_dir);

		Eigen::Matrix4f transf;
		transf.block(0,0,3,3)=rot;
		transf.block(0,3,3,1)=Eigen::Vector3f(0,0,0);
		pcl::PointCloud<PointT>::Ptr cloud_transf (new pcl::PointCloud<PointT>);
		pcl::transformPointCloud (*cloud, *cloud_transf, transf);
		
		// End generate random orientation

		// Corrupt with noise
		float cylinder_size=(max_pt.block(0,0,3,1)-min_pt.block(0,0,3,1)).norm();

		// End corrupt with noise
		ros::spinOnce();
		loop_rate.sleep();
		cloud_pub.publish(cloud_transf);
	}


	// Segment cylinder





	return (0);
}
