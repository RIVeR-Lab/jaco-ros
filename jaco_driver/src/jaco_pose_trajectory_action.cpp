/**
 *      _____
 *     /  _  \
 *    / _/ \  \
 *   / / \_/   \
 *  /  \_/  _   \  ___  _    ___   ___   ____   ____   ___   _____  _   _
 *  \  / \_/ \  / /  _\| |  | __| / _ \ | ++ \ | ++ \ / _ \ |_   _|| | | |
 *   \ \_/ \_/ /  | |  | |  | ++ | |_| || ++ / | ++_/| |_| |  | |  | +-+ |
 *    \  \_/  /   | |_ | |_ | ++ |  _  || |\ \ | |   |  _  |  | |  | +-+ |
 *     \_____/    \___/|___||___||_| |_||_| \_\|_|   |_| |_|  |_|  |_| |_|
 *             ROBOTICS™ 
 *
 *  File: jaco_pose_action.cpp
 *  Desc: Class for moving/querying jaco arm.
 *  Auth: Alex Bencz, Jeff Schmidt
 *
 *  Copyright (c) 2013, Clearpath Robotics, Inc. 
 *  All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Clearpath Robotics, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CLEARPATH ROBOTICS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Please send comments, questions, or patches to skynet@clearpathrobotics.com 
 *
 */

#include "jaco_driver/jaco_pose_trajectory_action.h"
#include <jaco_driver/KinovaTypes.h>
#include "jaco_driver/jaco_types.h"
#include <boost/foreach.hpp>

namespace jaco
{

JacoPoseTrajectoryActionServer::JacoPoseTrajectoryActionServer(JacoComm &arm_comm, ros::NodeHandle &n) : 
    arm(arm_comm), 
    as_(n, "arm_pose_trajectory", boost::bind(&JacoPoseTrajectoryActionServer::ActionCallback, this, _1), false)
{
    as_.start();
}

JacoPoseTrajectoryActionServer::~JacoPoseTrajectoryActionServer()
{

}

void JacoPoseTrajectoryActionServer::ActionCallback(const jaco_msgs::ArmPoseTrajectoryGoalConstPtr &goal)
{
	jaco_msgs::ArmPoseTrajectoryFeedback feedback;
	jaco_msgs::ArmPoseTrajectoryResult result;
	feedback.pose.header.frame_id = "/jaco_api_origin";
	result.pose.header.frame_id = "/jaco_api_origin";

	ROS_INFO("Got a cartesian trajectory goal for the arm");



	JacoPose cur_position;		//holds the current position of the arm
	geometry_msgs::PoseStamped local_pose;

	if (arm.Stopped())
	{
		arm.GetPosition(cur_position);
		local_pose.pose = cur_position.Pose();

		listener.transformPose(result.pose.header.frame_id, local_pose, result.pose);
		as_.setAborted(result);
		return;
	}

	bool is_first = true;
        BOOST_FOREACH(geometry_msgs::PoseStamped pose, goal->trajectory){
		if (ros::ok()
				&& !listener.canTransform("/jaco_api_origin", pose.header.frame_id,
						pose.header.stamp))
		{
			ROS_ERROR("Could not get transfrom from /jaco_api_origin to %s, aborting cartesian movement", pose.header.frame_id.c_str());
			return;
		}
		local_pose.header.frame_id = "/jaco_api_origin";
		listener.transformPose(local_pose.header.frame_id, pose, local_pose);

		JacoPose target(local_pose.pose);
		arm.SetPosition(target, 0, is_first);
		is_first = false;
	}

	ros::Rate r(10);
 
	const float tolerance = 0.05; 	//dead zone for position

	//while we have not timed out
	while (true)
	{
		ros::spinOnce();
		if (as_.isPreemptRequested() || !ros::ok())
		{
			arm.Stop();
			arm.Start();
			as_.setPreempted();
			return;
		}

		arm.GetPosition(cur_position);
		local_pose.pose = cur_position.Pose();

		listener.transformPose(feedback.pose.header.frame_id, local_pose, feedback.pose);

		if (arm.Stopped())
		{
			result.pose = feedback.pose;
			as_.setAborted(result);
			return;
		}

		as_.publishFeedback(feedback);

		int trajectory_size;
		arm.GetTrajectorySize(trajectory_size);
		if (trajectory_size == 0)
		{
			ROS_INFO("Cartesian Control Complete.");

			result.pose = feedback.pose;
			as_.setSucceeded(result);
			return;
		}

		r.sleep();
	}
}

}
