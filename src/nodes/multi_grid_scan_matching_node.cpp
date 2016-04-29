#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl_ros/point_cloud.h>
#include <nav_msgs/OccupancyGrid.h>
#include <tf/tf.h>

#include <ndt/conversion/convert.hpp>
#include <ndt/data/laserscan.hpp>
#include <ndt/matching/multi_grid_matcher_2D.hpp>
#include <ndt/visualization/multi_grid.hpp>

struct ScanMatcherNode {
    typedef ndt::matching::MultiGridMatcher2D   MultiGridMatcher2D;
    typedef ndt::visualization::MultiGrid2D     MultiGrid2D;
    typedef ndt::visualization::Point2D         Point2D;
    typedef pcl::PointCloud<pcl::PointXYZ>      PCLPointCloudType;

    ros::NodeHandle     nh;
    ros::Subscriber     sub;
    ros::Publisher      pub_pcl;
    ros::Publisher      pub_distr;

    ndt::data::LaserScan::Ptr   src;
    MultiGrid2D::ResolutionType resolution;

    ScanMatcherNode() :
        nh("~"),
        resolution{1.0, 1.0}
    {
        std::string topic_scan = "/scan";
        std::string topic_pcl  = "/matched";
        std::string topic_distr = "/distribution";
        nh.getParam("topic_scan", topic_scan);
        nh.getParam("topic_pcl", topic_pcl);
        nh.getParam("topic_distr", topic_distr);

        sub = nh.subscribe<sensor_msgs::LaserScan>(topic_scan, 1, &ScanMatcherNode::laserscan, this);
        pub_pcl = nh.advertise<PCLPointCloudType>(topic_pcl, 1);
        pub_distr = nh.advertise<nav_msgs::OccupancyGrid>(topic_distr, 1);

    }

    void laserscan(const sensor_msgs::LaserScanConstPtr &msg)
    {
        /// match old points to the current ones
        ndt::data::LaserScan dst;
        ndt::conversion::convert(msg, dst, false);
        if(!src) {
            src.reset(new ndt::data::LaserScan(dst));
        } else {
            MultiGridMatcher2D matcher;
            MultiGridMatcher2D::TransformType transform;
            double score = matcher.match(dst, *src, transform);

            PCLPointCloudType output;

            for(std::size_t i = 0 ; i < src->size ; ++i) {
                Point2D p_bar = transform * src->points[i];
                pcl::PointXYZ pcl_p;
                pcl_p.x = p_bar(0);
                pcl_p.y = p_bar(1);
                output.push_back(pcl_p);
            }

            ndt::data::LaserScan::PointType range = src->range();
            MultiGrid2D::SizeType  size = {std::size_t(range(0) / resolution[0]),
                                           std::size_t(range(1) / resolution[1])};

            cv::Mat distribution(500,500, CV_8UC3, cv::Scalar());
            MultiGrid2D grid(size, resolution, dst.min);
            grid.add(dst);
            ndt::visualization::renderMultiGrid(grid,
                                                dst.min,
                                                dst.max,
                                                distribution);
            /// output display
            if(score < 0.1){
                std::cout << "-------------------------------" << std::endl;
                std::cout << score << std::endl;
                std::cout << transform.translation() << std::endl;
                std::cout << transform.rotation() << std::endl;
                std::cout << "-------------------------------" << std::endl;
            }
            cv::cvtColor(distribution,  distribution, CV_BGR2GRAY);
            distribution *= 100.0 / 255.0;

            nav_msgs::OccupancyGrid distr_msg;
            distr_msg.header = msg->header;
            distr_msg.info.height = distribution.rows;
            distr_msg.info.width = distribution.cols;
            distr_msg.info.origin.position.x = dst.min(0);
            distr_msg.info.origin.position.y = dst.min(1);
            distr_msg.info.resolution = range(1) / distribution.rows;
            for(int i = 0 ; i < distribution.rows ; ++i) {
                for(int j = 0 ; j < distribution.cols ; ++j)
                    distr_msg.data.push_back(distribution.at<uchar>(distribution.rows - 1 - i, j));
            }

            output.header = pcl_conversions::toPCL(msg->header);
            pub_pcl.publish(output);
            pub_distr.publish(distr_msg);
            src.reset(new ndt::data::LaserScan(dst));
       }
   }
};




int main(int argc, char *argv[])
{
    ros::init(argc, argv, "ndt_multi_grid_matcher_node");
    ScanMatcherNode node;
    ros::spin();

    return 0;
}