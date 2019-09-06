#include "frene.hpp"

#include <fstream>
#include <iterator>
#include <iostream>
using namespace std;
#include "spline.h"
#include "generate_poly_trajectory.h"
#include "lane_list.h"
#include "trajectory_array.h"
#include <grid_map_ros/grid_map_ros.hpp>
#include <tf/tf.h>
#include <Eigen/Dense>

//tk::spline _splineX;
//tk::spline _splineY;


grid_map::Position applyTransform2(grid_map::Position in, double x, double y, double yaw){
  Eigen::Rotation2Dd rot(-yaw);
  Eigen::Vector2d trans(-x,-y);
  return rot*(in+trans);
  
}


Frene::Frene(std::vector< std::vector<double> >& laneListIn, double resamplingThreshold, /*std::vector< std::vector<double> >& xyResamplingList,*/ spline_struct& _splineX, spline_struct& _splineY, std::string ns, bool self )
{   
  if( laneListIn.size() <= 0 ){
    std::cout << "Error. Frene::Frene: laneListIn size must be larger than 0";
    return;
  }
  //std::cerr << "\n path size:" << laneListIn.size() ;


  std::vector< std::vector<double> > xyResamplingList;
  computeSplineFromXYListWithResamplingDenseData( laneListIn, resamplingThreshold, xyResamplingList, _splineX, _splineY );

  /*
  //for debugging 
  static bool firstFlag = true;
  if( strcmp ( ns.c_str(), "v1" ) == 0 )
    if( self == true )
      if( firstFlag ){
    ofstream fout( "/tmp/xyResamplingList.txt");
    for( std::vector< std::vector<double> >::iterator itr=xyResamplingList.begin(); itr != xyResamplingList.end(); itr++ ){
	 fout << (*itr)[0] << " " << (*itr)[1] << std::endl;
    }
    fout.close();
    firstFlag = false;
  }
  */

  //std::cout << "ResamplingData" << std::endl;
  //std::cout << "before" << std::endl;
  //LaneList::print(laneListIn);
  //std::cout << "after" << std::endl;
  //LaneList::print(xyResamplingList);
  //double _interpTick = 0.01;   //TODO must be moved to param.
  //std::vector< std::vector<double> > xyInterpList;
  //interpXYListFromSpline( xyInterpList, _interpTick, _totalArcLength ); 
  //std::cout << "spline" << std::endl;
  //LaneList::print(xyInterpList);  
    
};


//xyList x0 y0; x1 y1; ... 
//xyInterpList x0 y0; x1' y1'; ... 
void Frene::computeSplineFromXYListWithResamplingDenseData( std::vector< std::vector<double> >& xyList, double resamplingThreshold, 
							    std::vector< std::vector<double> >& xyResamplingListReturn,
							    spline_struct& _splineX, spline_struct& _splineY )
{
  
  //resampling
  double xp0 = 0.0;
  double yp0 = 0.0;
  
  std::vector< std::vector<double> > xyResamplingList; 
  xyResamplingList.clear();
  bool isFirstRun = true;
  int count = 0;
  for( std::vector< std::vector< double > >::iterator xy=xyList.begin(); xy != xyList.end(); xy++ ){        
    
    double xp1 = (*xy)[0];
    double yp1 = (*xy)[1];
    
    double distance=sqrt(pow(xp1-xp0,2.0) + pow(yp1-yp0,2.0));
    
    //std::cout << "Resampling " << xp1 << " " << yp1 << " " << distance << std::endl;    
    
    if( isFirstRun || distance > resamplingThreshold ){
      std::vector<double> point;
      point.push_back(xp1);
      point.push_back(yp1);
      xyResamplingList.push_back(point);
       
      xp0 = xp1;
      yp0 = yp1;
      isFirstRun = false;

      count ++;

      //std::cout << "Resampled " << xp1 << " " << yp1 << " " << distance << std::endl;    
      //TODO must be removed
      //if( count > 100 ){
      //	break;
      //}

    }else{
      //skip
    }    

  }
  
  _totalArcLength = computeSplineFromXYList( xyResamplingList, _splineX, _splineY );  

  xyResamplingListReturn = xyResamplingList;

}



double Frene::computeSplineFromXYList( std::vector< std::vector<double> >& xyList,  spline_struct& _splineX, spline_struct& _splineY )
{  
  /*
  //============================================================================  
  //check xy list size
  if( xyList.size() <= 0 ){
    std::cout << "Erorr: xyList.size() must be larger than0";
    return 0;
  }
  */
  if( xyList[0].size() < 2 ){
    std::cout << "Erorr: xyList[0].size() must be larger than 2";
    return 0;
  }
  
  //first compute distance between each points and also total distance
  double xp0  = 0.0;
  double yp0  = 0.0;
  double sumd = 0.0;
  bool isFirstRun = true;
  std::vector<double> dvec;
  for( std::vector< std::vector< double > >::iterator xy=xyList.begin(); xy != xyList.end(); xy++ ){    
    double xp1 = (*xy)[0];
    double yp1 = (*xy)[1];
    double d = sqrt(pow(xp1-xp0,2.0)+pow(yp1-yp0,2.0));        
    
    if( isFirstRun ){
      d = 0.0;
      isFirstRun = false;
    } 
    
    dvec.push_back(d);    
    //ROS_INFO_STREAM( "xp0 yp0 xp1 yp1 sumd: " << xp0 << " " << yp0 << " " << xp1 << " " << yp1 << " " << sumd  );
    
    xp0 = xp1;
    yp0 = yp1;
    sumd = sumd +  d;    
  }
  
  //compute arc length s and make s vector
  std::vector<double> svec;  
  double s0 = 0.0;
  for( std::vector< double >::iterator d=dvec.begin(); d != dvec.end(); d++ ){      
    double s1 = s0 + (*d);///sumd;
    svec.push_back(s1);
    s0 = s1;

    //ROS_INFO_STREAM( "*d s1, sumd: " << *d << " " << s1 << " " << sumd  );
  }
  
  //make x, y position vector
  std::vector<double> yvec;
  std::vector<double> xvec;
  for( std::vector< std::vector< double > >::iterator xy=xyList.begin(); xy != xyList.end(); xy++ ){    
    double xpoint = (*xy)[0];
    double ypoint = (*xy)[1];
    
    xvec.push_back(xpoint);
    yvec.push_back(ypoint);
  }

  //now spline interpolation
  _splineX.set_points(svec,xvec);
  _splineY.set_points(svec,yvec);   
  

  
  //for debug
  static bool firstFlag = true;
  if( firstFlag ){
    ofstream fout( "/tmp/setpoints.txt");
    for( int i=0; i < svec.size(); i++ ){
      fout << svec[i] << " " << xvec[i] << " " << yvec[i] << std::endl;
    }
    fout.close();
    firstFlag = false;
  }
 
  //return total length
  //std::cout<<sumd<< std::endl;
  return sumd;
}

void Frene::convertFromCartesianState(double t, double current_x, double current_y, double theta, double vx, double ax, double kx, double dtTheta, FreneState& freneState, double arcLengthTickMeter,  spline_struct& _splineX, spline_struct& _splineY, Eigen::Vector2d &next_subgoal, double distance_to_subgoal_ahead, std::string ns, bool self )
{
  double s0, d0, k0, dsk0;
  double distanceAbsMin = 1e+14;
  FreneState freneStateMin;
  Eigen::VectorXd selected;
  Eigen::Vector2d selected_rr_ahead(2);
  Eigen::VectorXd selected_nr_ahead;
  double selected_s;

  
  /*
  if( strcmp ( ns.c_str(), "v2" ) == 0 )
    if( self == true )
      std::cerr << "\n\n arclength: " << _totalArcLength << " " << current_x << " " << current_y << " " ;
  */

  
  //for debug
  static bool firstFlag = true;
  //if (id_robot==1)
  if( strcmp ( ns.c_str(), "v1" ) == 0 )
  if( self == true )
  if( firstFlag ){
    ofstream fout( "/tmp/lineState1.txt");
    for( double s=0; s < _totalArcLength; s+= arcLengthTickMeter ){  
      LineState lineState;
      computeLineStateAtInterp(s, lineState, _splineX, _splineY );    
      fout << s << " " << lineState.kr << " " << lineState.dskr << " " << lineState.rr(0) << " " << lineState.rr(1) << std::endl;
      
    }
    fout.close();
    firstFlag = false;
  }
  for( double s=0; s < _totalArcLength; s+= arcLengthTickMeter ){  
    //for( double s=0; s < 1.0; s+= arcLengthTickMeter/_totalArcLength ){


    
    //Get the point to be followed 1.0 m ahead.
    //double distance_to_subgoal_ahead = 2.0 ;
    double s_ahead = s + distance_to_subgoal_ahead;   // 1.0 m
    double xr_ahead = _splineX(s_ahead);
    double yr_ahead = _splineY(s_ahead);
    //convert the values to FreneState
    Eigen::Vector2d rr_ahead(2); rr_ahead << xr_ahead, yr_ahead;
      
    
      
    
    LineState lineState;
    computeLineStateAt(s, lineState, _splineX, _splineY );
    
    Eigen::VectorXd rr=lineState.rr;
    Eigen::VectorXd tr=lineState.tr;
    Eigen::VectorXd nr=lineState.nr;
    double    thetar = lineState.thetar;
    double    kr = lineState.kr;
    Eigen::VectorXd dstr=lineState.tr;
    double    dskr=lineState.dskr;
    
    Eigen::VectorXd current_p(2);
    current_p << current_x, current_y;
    double    distance = (current_p - rr).norm();
    double    distanceAbs = fabs(distance);

    
    
    Eigen::VectorXd tx(2); tx << cos(theta), sin(theta);

    double deltaTheta   = theta - thetar;
    double    d = (current_p - rr).dot( nr);
    
    double dtd = vx*sin(deltaTheta);
    double dts = cos(deltaTheta)/(1 - kr*d)*vx;
    double dsd = (1-kr*d)*tan(deltaTheta);

    double ddsd = -(dskr*d+kr*dsd)*tan(deltaTheta) + (1-kr*d)/(cos(deltaTheta)*cos(deltaTheta))*(kx*(1-kr*d)/cos(deltaTheta)-kr);
        
    Eigen::VectorXd dstx(2); dstx << -sin(theta)/dts*dtTheta, cos(theta)/dts*dtTheta;  
    Eigen::VectorXd deltadsTrdsTx = dstr - dstx;    
    double dsdeltaTheta = atan2( deltadsTrdsTx(1), deltadsTrdsTx(0)); 
    
    double ddts = cos(deltaTheta)/(1 - kr*d)*(ax - dts*dts/cos(deltaTheta)*( (1-kr*d)*tan(deltaTheta)*dsdeltaTheta-(dskr*d+kr*dsd) )) ;
    
    double ddtd = ddsd*dts*dts + dsd*ddts;

    
    //if( strcmp ( ns.c_str(), "v1" ) == 0 ) if( self == true )
    //					     std::cerr << "\n path: " << lineState.rr[0] << " " <<lineState.rr[1] << " " <<  s ;
    
    //std::cout << "s x y rr0 rr1 vx" << s << " " << x << " " << y << " " << rr(0) << " " << rr(1) << " " << vx << std::endl;
    //std::cout << "path: " << s << " " << kr << " " << thetar << " " << dskr << std::endl;
    
    if( distanceAbs < distanceAbsMin ){

      //if (id_robot==1)
      //if( strcmp ( ns.c_str(), "v1" ) == 0 )
      //std::cerr << " s: " << s <<" distance: " << distanceAbs << " distanceprev: " << distanceAbsMin << "   " ;  
      
      distanceAbsMin = distanceAbs;
      freneStateMin.td = t;
      freneStateMin.ts = t;
      freneStateMin.d = d;
      freneStateMin.dtd = dtd;
      freneStateMin.ddtd = ddtd;
      freneStateMin.s = s;
      freneStateMin.dts = dts;
      freneStateMin.ddts = ddts;

      selected = lineState.rr;

      selected_s = s_ahead ;
      selected_rr_ahead = rr_ahead;
      selected_nr_ahead = nr;
    }
    
    
  }

  //Transform subgoal ahead to x, y from frenet
  double d = freneStateMin.d;
  Eigen::VectorXd nr = selected_nr_ahead;
  Eigen::VectorXd p = selected_rr_ahead + d * selected_nr_ahead;
  double x = p(0);
  double y = p(1);

  Eigen::Vector2d next_( p(0), p(1) ) ;
  next_subgoal =  next_ ;
  
  //std::cerr << "\ns: " << selected_s << " current: " << current_x << " " << current_y << "  subgoal: " << next_subgoal(0) << " " << next_subgoal(1) ;
  
  freneState = freneStateMin;  
  //std::cout << "s0 is " << freneState.s << " " << interpTick << std::endl;  
  //std::cout << freneState.s << " " << freneState.d << " " << freneState.kr 
  //	    << " " << freneState.s
  //	    << " " << freneState.dts
  //	    << " " << freneState.ddts << std::endl;
  //if (id_robot==1)

  /*
  if( strcmp ( ns.c_str(), "v1" ) == 0 )
    if( self == true )
    std::cerr << "\n" << "s0 is " << freneState.s << " " << selected[0] << " " << selected[1] << " current_pos: " << current_x << " " << current_y;
  */
}



void Frene::computeLineStateAtInterp( double s, LineState& state, spline_struct& _splineX, spline_struct& _splineY )
{
  LineState state0;
  double deltas = 1;
  double L = 10;
  double krsum = 0;
  double count = 0.0;
  for( int i=0; i < L/deltas; i++ ){
    double s0 = (s - L/2.0) + deltas * i;
    computeLineStateAt(s0, state0, _splineX, _splineY);
    krsum += state0.kr;
    count++;
  }

  computeLineStateAt(s, state, _splineX, _splineY);  
  //state.kr = krsum / count;
}


void Frene::computeLineStateAt( double s, LineState& state, spline_struct& _splineX, spline_struct& _splineY )
{
  //read values from list
  double xr=_splineX(s);//_polyX.value(s);//
  double yr=_splineY(s);//_polyY.value(s);//
  double sr=s;    
  double dsxr  = _splineX.deriv(1,s);
  double dsyr  = _splineY.deriv(1,s);
  double ddsxr = _splineX.deriv(2,s);
  double ddsyr = _splineY.deriv(2,s);
  double dddsxr= _splineX.deriv(3,s);
  double dddsyr= _splineY.deriv(3,s);
  
  //convert the values to FreneState
  Eigen::Vector2d rr(2); rr << xr, yr;
  
  Eigen::Matrix2d rot90; rot90 << 0, -1, 1, 0;
  double thetar = atan2(dsyr,dsxr);
  double dsThetar = pow(cos(thetar),2.0) * ((ddsyr*dsxr-dsyr*ddsxr)/pow(dsxr,2.0));
  
  //arma::vec tr(2); tr << cos(thetar) << sin(thetar);
  Eigen::Vector2d tr; tr << dsxr,  dsyr; tr.normalized();
  Eigen::VectorXd nr = rot90*tr;    
  Eigen::Vector2d dstr; dstr << -sin(thetar)*dsThetar, cos(thetar)*dsThetar;
  
  double dsxr2 = pow( dsxr, 2.0 );
  double dsyr2 = pow( dsyr, 2.0 );
  double kr = (dsxr*ddsyr - dsyr*ddsxr) * pow( dsxr2 + dsyr2, -3.0/2.0);
  double dskr = (ddsxr*ddsyr + dsxr*dddsyr -ddsyr*ddsxr - dsyr*dddsxr)*pow( dsxr2 + dsyr2, -3.0/2.0)
    -3*(dsxr*ddsyr-dsyr*ddsxr)*pow(dsxr2+dsyr2,-5.0/2.0)*(dsxr*ddsxr+dsyr*ddsyr);

  //for debug
  //kr   = ddsxr/pow(1+dsxr2, 3.0/2.0);
  //dskr = ddsyr/pow(1+dsyr2, 3.0/2.0);
  //kr   = _splineX.deriv(2,s)/pow(1+pow(_splineX.deriv(1,s),2.0), 1.5);
  //dskr = _splineY.deriv(2,s)/pow(1+pow(_splineY.deriv(1,s),2.0), 1.5);
  
  state.rr = rr;
  state.nr = nr;
  state.tr = tr;
  state.dstr = dstr;
  state.dskr = dskr;
  state.kr = kr;
  state.thetar = thetar;
  
}

void Frene::convertFromFreneState(FreneState& freneState, double& x, double& y, double& theta, double& vx, double& ax, double& kx, double& dtTheta, spline_struct& _splineX, spline_struct& _splineY)
{  
  double d = freneState.d;
  double dtd = freneState.dtd;   
  double ddtd = freneState.ddtd;   
  double s = freneState.s;   
  double dts = freneState.dts;   
  double ddts = freneState.ddts;   
  
  LineState lineState;
  computeLineStateAtInterp( s, lineState, _splineX, _splineY );
  double kr = lineState.kr;
  double dskr = lineState.dskr;
  Eigen::VectorXd dstr = lineState.dstr;
  double thetar = lineState.thetar;
  Eigen::VectorXd rr = lineState.rr;
  Eigen::VectorXd nr = lineState.nr;
  
  double dsd = dtd/dts;
  double ddsd = (ddtd - dsd*ddts)/pow(dts,2.0);
  
  double deltaTheta = atan( dsd/(1-kr*d) );
  
  double dsTheta = (ddsd+(dskr*d+kr*dsd)*tan(deltaTheta))*pow(deltaTheta,2.0)/(1-kr*d) + kr;
  kx = cos(deltaTheta)/(1-kr*d)*dsTheta;  
  //kx = pow(cos(deltaTheta),3.0)/pow(1-kr*d,2.0)*(ddsd+(dskr*d+kr*dsd)*tan(deltaTheta)) + cos(deltaTheta)/(1-kr*d)*kr; 
  
  vx = sqrt( pow(1-kr*d,2.0)*pow(dts,2.0) + pow(dtd,2.0) );
  Eigen::VectorXd p = rr + d*nr;
  
  x = p(0);
  y = p(1);

  
  
  theta = deltaTheta + thetar;  
  dtTheta = kx*vx;
  
  //arma::vec dstx(2); dstx << -sin(theta)/dts*dtTheta << cos(theta)/dts*dtTheta;  
  //arma::vec deltadsTrdsTx = dstr - dstx;
  //double dsdeltaTheta = atan2( deltadsTrdsTx(1), deltadsTrdsTx(0) ); 
  double dsdeltaTheta = kx*(1-kr*d)/cos(deltaTheta)-kr;
  
  ax = ddts*(1-kr*d)/cos(deltaTheta) + dts*dts/cos(deltaTheta)*((1-kr*d)*tan(deltaTheta)*dsdeltaTheta-(dskr*d+kr*dsd));  
  
  //std::cout << s << " " << kr << " " << dskr << " " << thetar << " " << std::endl;
  
}

bool Frene::isSmallerThanOrApproxEqual( double val1, double val2, double eps )
{
  if( val1 < val2 || fabs(val1-val2) < eps ){
    return true;
  }
  else{
    return false;
  }
}

/*
void Frene::generateLateralAndLongitudinalTrajectoryStopping( FreneState& initialState,
							       std::vector< std::vector< std::vector<double> > >& trajectoryArrayList,
							       std::vector< std::vector< std::vector<double> > >& paramArrayList,
							       double desiredLongSpeed, double desiredLongAcceleration, 
							       double timeMinSec, double timeMaxSec, double timeDeltaSec, 
							       double desiredLongPosition, double desiredLongPositionMinus, double desiredLongPositionPlus, double desiredLongPositionDelta,
							       double desiredLatPosition, double desiredLatPositionMinus, double desiredLatPositionPlus, double desiredLatPositionDelta,
							       double tickTime
							       )
{
  generateLateralAndLongitudinalTrajectoryFollowing( initialState,
						     trajectoryArrayList,
						     paramArrayList,
						     desiredLongSpeed, desiredLongAcceleration, 
						     timeMinSec, timeMaxSec, timeDeltaSec, 
						     desiredLongPosition, desiredLongPositionMinus, desiredLongPositionPlus, desiredLongPositionDelta,
						     desiredLatPosition, desiredLatPositionMinus, desiredLatPositionPlus, desiredLatPositionDelta,
						     tickTime );						     
}
*/


void Frene::generateLateralAndLongitudinalTrajectoryFollowing( FreneState& initialState,
							       std::vector< std::vector< std::vector<double> > >& trajectoryArrayList,
							       std::vector< std::vector< std::vector<double> > >& paramArrayList,
							       double desiredLongSpeed, double desiredLongAcceleration, 
							       double timeMinSec, double timeMaxSec, double timeDeltaSec, 
							       double desiredLongPosition, double desiredLongPositionMinus, double desiredLongPositionPlus, double desiredLongPositionDelta,
							       double desiredLatPosition, double desiredLatPositionMinus, double desiredLatPositionPlus, double desiredLatPositionDelta,
							       double tickTime
							       )
{
  trajectoryArrayList.clear();
  
  //for test
  //double scurrent  = initialState.s;
  //double dscurrent = initialState.dts;
  //double tau=0.1;
  //double sd = 30;//desiredLongPosition;
  //double sd1= scurrent + max(dscurrent,0.0)*tau;
  //double sd2= sd;
  //double k = 10;
  //double amplitude = 1.0;
  //if( scurrent  > sd -k ){
  //desiredLongPosition = sd1;
  //}else{
  //desiredLongPosition = sd2;
  //}
  
  for( double tf = timeMinSec; tf < timeMaxSec; tf += timeDeltaSec ){  
    for( double sf = desiredLongPosition + desiredLongPositionMinus; isSmallerThanOrApproxEqual( sf, desiredLongPosition + desiredLongPositionPlus, 1e-04); sf += desiredLongPositionDelta ){      
      
      std::vector< std::vector<double> > trajectoryLongitudinal;
      std::vector< std::vector<double> > paramLongitudinal;

      GeneratePolyTrajectory::startComputeQuinticPolynomial( initialState.s, initialState.dts, initialState.ddts, 
      						     tf, sf, desiredLongSpeed, desiredLongAcceleration,  		   
      						     trajectoryLongitudinal, paramLongitudinal, tickTime );            
      for( double df = desiredLatPosition + desiredLatPositionMinus; isSmallerThanOrApproxEqual(df, desiredLatPosition+desiredLatPositionPlus, 1e-04); df += desiredLatPositionDelta ){      	
	std::vector< std::vector<double> > trajectoryLateral;	
	std::vector< std::vector<double> > paramLateral;
	GeneratePolyTrajectory::startComputeQuinticPolynomial( initialState.d, initialState.dtd, initialState.ddtd, 
							       tf, df, 0.0, 0.0, 							   
							       trajectoryLateral, paramLateral, tickTime );	
	
	std::vector< std::vector<double> > trajectoryMerge;
	std::vector< std::vector<double> > paramMerge;
	TrajectoryArray::mergeTrajectoryList( trajectoryLongitudinal, trajectoryLateral, trajectoryMerge );
	TrajectoryArray::mergeTrajectoryList( paramLongitudinal, paramLateral, paramMerge );
	
	//std::cout << "Merge" << std::endl;
	//LaneList::print( trajectoryMerge );
	
	trajectoryArrayList.push_back( trajectoryMerge );
	paramArrayList.push_back( paramMerge );
	
      }
    }
  }
}




void Frene::generateLateralAndLongitudinalTrajectoryVelocityKeeping( FreneState& initialState,
								     std::vector< std::vector< std::vector<double> > >& trajectoryArrayList,
								     std::vector< std::vector< std::vector<double> > >& paramArrayList,
								     double timeMinSec, double timeMaxSec, double timeDeltaSec, 
								     double desiredSpeed, double desiredSpeedMinus, double desiredSpeedPlus, double desiredSpeedDelta,
								     double desiredLatPosition, double desiredLatPositionMinus, double desiredLatPositionPlus, double desiredLatPositionDelta,
								     double tickTime
								     )
{
  trajectoryArrayList.clear();
  for( double tf = timeMinSec; tf < timeMaxSec; tf += timeDeltaSec ){
    //std::cerr << "\ntf: " << tf << " min: " << timeMinSec << " max: " << timeMaxSec << " delta: " << timeDeltaSec ;
    for( double dtsf = desiredSpeed + desiredSpeedMinus; isSmallerThanOrApproxEqual( dtsf, desiredSpeed + desiredSpeedPlus, 1e-04); dtsf += desiredSpeedDelta ){      
      //std::cerr << "\ndtsf: " << dtsf << "= " << desiredSpeed << "+" << desiredSpeedMinus << " condition: " << isSmallerThanOrApproxEqual( dtsf, desiredSpeed + desiredSpeedPlus, 1e-04) << " max: " << desiredSpeed + desiredSpeedPlus << " delta: " << desiredSpeedDelta ;
      std::vector< std::vector<double> > trajectoryLongitudinal;
      std::vector< std::vector<double> > paramLongitudinal;
      //std::cout<<"s: "<< initialState.s<< " dts:"<< initialState.dts<<" ddts:"<<initialState.ddts<<std::endl;
      GeneratePolyTrajectory::startComputeQuarticPolynomial( initialState.s, initialState.dts, initialState.ddts, 
							     tf, dtsf, 0.0, 		   
							     trajectoryLongitudinal, paramLongitudinal, tickTime );            
      for( double df = desiredLatPosition + desiredLatPositionMinus; isSmallerThanOrApproxEqual(df, desiredLatPosition+desiredLatPositionPlus, 1e-04); df += desiredLatPositionDelta ){      	
	std::vector< std::vector<double> > trajectoryLateral;	
	std::vector< std::vector<double> > paramLateral;
	GeneratePolyTrajectory::startComputeQuinticPolynomial( initialState.d, initialState.dtd, initialState.ddtd, 
							       tf, df, 0.0, 0.0, 							   
							       trajectoryLateral, paramLateral, tickTime );	
	
	std::vector< std::vector<double> > trajectoryMerge;
	std::vector< std::vector<double> > paramMerge;
	TrajectoryArray::mergeTrajectoryList( trajectoryLongitudinal, trajectoryLateral, trajectoryMerge );
	TrajectoryArray::mergeTrajectoryList( paramLongitudinal, paramLateral, paramMerge );
	
	//std::cout << "Merge" << std::endl;
	//LaneList::print( trajectoryMerge );
	
	trajectoryArrayList.push_back( trajectoryMerge );
	paramArrayList.push_back( paramMerge );
	
      }
    }
  }
}



void Frene::convertMergedTrajectoryArrayToCartesian( std::vector< std::vector< std::vector<double> > >& trajectoryArrayList, 
						     std::vector< std::vector< std::vector<double> > >& trajectoryArrayCartesianList, spline_struct& _splineX, spline_struct& _splineY )
{
  trajectoryArrayCartesianList.clear();
  
  for( std::vector< std::vector< std::vector<double> > >::iterator itr0 = trajectoryArrayList.begin(); itr0 != trajectoryArrayList.end(); itr0++  ){
    
    std::vector< std::vector<double> > trajectoryCartesian;
    for( std::vector< std::vector< double > >::iterator itr1 = itr0->begin(); itr1 != itr0->end(); itr1++  ){    
      FreneState freneState;
      if( itr1->size() < FreneState::size() ){
	std::cout << "Error: Merged trajectory size is incorrect" << std::endl;
      }
      
      freneState.ts     = (*itr1)[0];
      freneState.s     = (*itr1)[1];
      freneState.dts   = (*itr1)[2];
      freneState.ddts  = (*itr1)[3];
      freneState.dddts = (*itr1)[4];	
      freneState.sumdddts = (*itr1)[5];	      
      freneState.td     = (*itr1)[6];
      freneState.d     = (*itr1)[7];
      freneState.dtd   = (*itr1)[8];
      freneState.ddtd  = (*itr1)[9];
      freneState.dddtd = (*itr1)[10];
      freneState.sumdddtd = (*itr1)[11];
      
      //std::cout <<  freneState.ts << " " << freneState.d << " " << freneState.s << std::endl;      
      double x, y, theta, dtTheta, vx, ax, kx;
      convertFromFreneState(freneState, x,y,theta,vx,ax,kx,dtTheta, _splineX, _splineY);	
      double t = freneState.ts;

      //if ( itr1 == itr0->begin())
      //std::cerr << "frene_path_coordinates: " <<  x << " " << y << std::endl;
      
      std::vector<double> point;
      point.push_back(x);
      point.push_back(y);
      point.push_back(t);
      point.push_back(theta);
      point.push_back(dtTheta);
      point.push_back(vx);
      point.push_back(ax);
      point.push_back(kx);
      point.push_back(freneState.ts);
      point.push_back(freneState.s);
      point.push_back(freneState.dts);
      point.push_back(freneState.ddts);
      point.push_back(freneState.dddts);
      point.push_back(freneState.sumdddts);
      point.push_back(freneState.td);
      point.push_back(freneState.d);
      point.push_back(freneState.dtd);
      point.push_back(freneState.ddtd);
      point.push_back(freneState.dddtd);
      point.push_back(freneState.sumdddtd);
      
      double centrifugalForce = kx*pow(vx,2.0);
      point.push_back(centrifugalForce);
      
      
      trajectoryCartesian.push_back(point);      
    }
    trajectoryArrayCartesianList.push_back(trajectoryCartesian);
  }
}


/*

void Frene::computeInitialStateFromParam(double t, std::vector< std::vector< std::vector<double> > >& paramArray, FreneState& state )
{
  
  std::vector< std::vector< std::vector< double > > >::iterator itr=paramArray.begin();
    
  //params for longitudinal  
  double a0=(*itr)[0][0];
  double a1=(*itr)[0][1];
  double a2=(*itr)[0][2];
  double a3=(*itr)[0][3];
  double a4=(*itr)[0][4];
  GeneratePolyTrajectory::computeQuarticValueFromParam( t, a0, a1, a2, a3, a4, 
							state.s, state.dts, state.ddts, state.dddts );
  //params for lateral
  double b0=(*itr)[0][5];
  double b1=(*itr)[0][6];
  double b2=(*itr)[0][7];
  double b3=(*itr)[0][8];
  double b4=(*itr)[0][9];
  double b5=(*itr)[0][10];
  GeneratePolyTrajectory::computeQuinticValueFromParam( t, b0, b1, b2, b3, b4, b5, 
							state.d, state.dtd, state.ddtd, state.dddtd );  
  
}


void Frene::computeInitialStateFromPreviousTrajectory( double xCurrent, double yCurrent, double vxCurrent, std::vector< std::vector< std::vector< double > > >& previousTrajectoryArray, 
						       double& x, double& y, double& t, double& theta, double& dtTheta, double& vx, double& ax, double& kx )
{  
  double distanceMin = 1e+14;

  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=previousTrajectoryArray.begin(); itr1 != previousTrajectoryArray.end(); itr1++ ){    
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){    
      if( itr2->size() < 8 ){
	return;	
      }
      double xt = (*itr2)[0];
      double yt = (*itr2)[1];  
      double vxt = (*itr2)[5];  
      
      double distance = sqrt(pow( xCurrent-xt, 2.0) + pow( yCurrent-yt, 2.0 ));
      //double distance = sqrt(pow( vxCurrent-vxt, 2.0) );
      
      if( distance < distanceMin ){
	distanceMin = distance;
	
	x = (*itr2)[0];
	y = (*itr2)[1];
	t = (*itr2)[2];
	theta = (*itr2)[3];
	dtTheta = (*itr2)[4];
	vx = (*itr2)[5];
	ax = (*itr2)[6];
	kx = (*itr2)[7];
	
      }	
    }    
  } 
  
  //std::cout << xCurrent << " " << yCurrent << std::endl;
  //std::cout << x << " " << y << " " << t << " " << theta << " " << dtTheta << " " << vx << " " << ax << " " << kx << std::endl;

}


void Frene::computeInitialStateFromPreviousTrajectoryWithSplineResampling(double xCurrent, double yCurrent, 
									   std::vector< std::vector< std::vector< double > > >& previousTrajectoryArray, 
									  double& x, double& y, double& t, double& theta, double& dtTheta, double& vx, double& ax, double& kx,
									  double resamplingTickTime,
									  double timeFromPreviousCompute,
									  bool useTimeMode
									  )
{  

  std::vector< double > xvec;
  std::vector< double > yvec;
  std::vector< double > tvec;
  std::vector< double > thetavec;
  std::vector< double > dtThetavec;
  std::vector< double > vxvec;
  std::vector< double > axvec;
  std::vector< double > kxvec;
  
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=previousTrajectoryArray.begin(); itr1 != previousTrajectoryArray.end(); itr1++ ){    
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){    
      if( itr2->size() < 8 ){
	return;	
      }
      double xt = (*itr2)[0];
      double yt = (*itr2)[1];  
      double tt = (*itr2)[2];  
      double thetat = (*itr2)[3];  
      double dtThetat = (*itr2)[4];  
      double vxt = (*itr2)[5];  
      double axt = (*itr2)[6];  
      double kxt = (*itr2)[7];

      xvec.push_back(xt);
      yvec.push_back(yt);
      tvec.push_back(tt);
      thetavec.push_back(thetat);
      dtThetavec.push_back(dtThetat);
      vxvec.push_back(vxt);
      axvec.push_back(axt);
      kxvec.push_back(kxt);
    }
  }
  
  //now spline interpolation
  tk::spline splineX;
  tk::spline splineY;
  tk::spline splineT;
  tk::spline splineTheta;
  tk::spline splineDtTheta;
  tk::spline splineVx;
  tk::spline splineAx;
  tk::spline splineKx;

  splineX.set_points(tvec,xvec);
  splineY.set_points(tvec,yvec);   
  splineT.set_points(tvec,tvec);   
  splineTheta.set_points(tvec,thetavec);   
  splineDtTheta.set_points(tvec,dtThetavec);   
  splineVx.set_points(tvec,vxvec);   
  splineAx.set_points(tvec,axvec);   
  splineKx.set_points(tvec,kxvec);   
  
  //find the nearest point in resampling spline 
  double tf = tvec.back();
  double distanceMin = 1e+14;
  for( double t0=0; t0 < tf; t0 += resamplingTickTime ){  
    
    double xt = splineX(t0);
    double yt = splineY(t0);
    double tt = splineT(t0);
    double thetat = splineTheta(t0);
    double dtThetat = splineDtTheta(t0);
    double vxt = splineVx(t0);
    double axt = splineAx(t0);
    double kxt = splineKx(t0);
    
    double distance = sqrt( pow(yCurrent-yt,2.0) + pow(xCurrent-xt,2.0) );
    if( distance < distanceMin ){
      distanceMin = distance;
      x = xt;
      y = yt;
      t = tt;
      theta = thetat;
      dtTheta = dtThetat;
      vx = vxt;
      ax = axt;
      kx = kxt;      
    }    
  }
  
  if( useTimeMode ){
    double tt = timeFromPreviousCompute;
    x = splineX(tt);
    y = splineY(tt);
    t = splineT(tt);
    theta = splineTheta(tt);
    dtTheta = splineDtTheta(tt);
    vx = splineVx(tt);
    ax = splineAx(tt);
    kx = splineKx(tt);  
  }
}

*/


void Frene::computeInitialStateDirectFromPreviousTrajectoryWithSplineResampling(double xCurrent, double yCurrent, 
										std::vector< std::vector< std::vector< double > > >& previousTrajectoryArray, 
										FreneState& freneState,
										double resamplingTickTime
									  )
{  

  std::vector< double > xvec;
  std::vector< double > yvec;
  std::vector< double > tvec;
  std::vector< double > thetavec;
  std::vector< double > dtThetavec;
  std::vector< double > vxvec;
  std::vector< double > axvec;
  std::vector< double > kxvec;

  std::vector< double > tsvec;
  std::vector< double > svec;
  std::vector< double > dtsvec;
  std::vector< double > ddtsvec;
  std::vector< double > dddtsvec;
  std::vector< double > tdvec;
  std::vector< double > dvec;
  std::vector< double > dtdvec;
  std::vector< double > ddtdvec;
  std::vector< double > dddtdvec;
  std::vector< double > centrifugalForcevec;

  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=previousTrajectoryArray.begin(); itr1 != previousTrajectoryArray.end(); itr1++ ){    
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){          
  
      if( itr2->size() < 21 ){
	return;	
      }
      double xt = (*itr2)[0];
      double yt = (*itr2)[1];  
      double tt = (*itr2)[2];  
      double thetat = (*itr2)[3];  
      double dtThetat = (*itr2)[4];  
      double vxt = (*itr2)[5];  
      double axt = (*itr2)[6];  
      double kxt = (*itr2)[7];
      double ts = (*itr2)[8];
      double s = (*itr2)[9];
      double dts = (*itr2)[10];
      double ddts = (*itr2)[11];
      double dddts = (*itr2)[12];
      double sumdddts = (*itr2)[13];
      double td = (*itr2)[14];
      double d = (*itr2)[15];
      double dtd = (*itr2)[16];
      double ddtd = (*itr2)[17];
      double dddtd = (*itr2)[18];
      double sumdddtd = (*itr2)[19];
      double centrifugalForce = (*itr2)[20];
      
      xvec.push_back(xt);
      yvec.push_back(yt);
      tvec.push_back(tt);
      thetavec.push_back(thetat);
      dtThetavec.push_back(dtThetat);
      vxvec.push_back(vxt);
      axvec.push_back(axt);
      kxvec.push_back(kxt);
      tsvec.push_back(ts);      
      svec.push_back(s);      
      dtsvec.push_back(dts);      
      ddtsvec.push_back(ddts);      
      dddtsvec.push_back(dddts);      
      tdvec.push_back(td);      
      dvec.push_back(d);      
      dtdvec.push_back(dtd);      
      ddtdvec.push_back(ddtd);      
      dddtdvec.push_back(ddtd);      
      centrifugalForcevec.push_back(centrifugalForce);      
    }
  }
  /*
  //now spline interpolation
  tk::spline splineX;
  tk::spline splineY;
  tk::spline splineT;
  tk::spline splineTheta;
  tk::spline splineDtTheta;
  tk::spline splineVx;
  tk::spline splineAx;
  tk::spline splineKx;
  tk::spline splineTs;  
  tk::spline splineS;  
  tk::spline splineDts;  
  tk::spline splineDdts;  
  tk::spline splineDddts;  
  tk::spline splineTd;  
  tk::spline splineD;  
  tk::spline splineDtd;  
  tk::spline splineDdtd;    
  tk::spline splineDddtd;    
  tk::spline splineCentrifugalForce;    
  */

  spline_struct splineX;
  spline_struct splineY;
  spline_struct splineT;
  spline_struct splineTheta;
  spline_struct splineDtTheta;
  spline_struct splineVx;
  spline_struct splineAx;
  spline_struct splineKx;
  spline_struct splineTs;  
  spline_struct splineS;  
  spline_struct splineDts;  
  spline_struct splineDdts;  
  spline_struct splineDddts;  
  spline_struct splineTd;  
  spline_struct splineD;  
  spline_struct splineDtd;  
  spline_struct splineDdtd;    
  spline_struct splineDddtd;    
  spline_struct splineCentrifugalForce;    
  
  splineX.set_points(tvec,xvec);
  splineY.set_points(tvec,yvec);   
  splineT.set_points(tvec,tvec);   
  splineTheta.set_points(tvec,thetavec);   
  splineDtTheta.set_points(tvec,dtThetavec);   
  splineVx.set_points(tvec,vxvec);   
  splineAx.set_points(tvec,axvec);   
  splineKx.set_points(tvec,kxvec);   
  splineTs.set_points(tvec,tsvec);   
  splineS.set_points(tvec,svec);   
  splineDts.set_points(tvec,dtsvec);   
  splineDdts.set_points(tvec,ddtsvec);   
  splineDddts.set_points(tvec,dddtsvec);   
  splineTd.set_points(tvec,tdvec);   
  splineD.set_points(tvec,dvec);   
  splineDtd.set_points(tvec,dtdvec);   
  splineDdtd.set_points(tvec,ddtdvec);   
  splineDddtd.set_points(tvec,dddtdvec);   
  splineCentrifugalForce.set_points(tvec,centrifugalForcevec);   
  
  //find the nearest point in resampling spline 
  double tf = tvec.back();
  double distanceMin = 1e+14;
  for( double t0=0; t0 < tf; t0 += resamplingTickTime ){  
    
    double xt = splineX(t0);
    double yt = splineY(t0);
    double tst = splineTs(t0);
    double st = splineS(t0);    
    double dtst = splineDts(t0);    
    double ddtst = splineDdts(t0);    
    double dddtst = splineDddts(t0);    
    double tdt = splineTd(t0);
    double dt = splineD(t0);    
    double dtdt = splineDtd(t0);    
    double ddtdt = splineDdtd(t0);    
    double dddtdt = splineDddtd(t0);    
    
    double distance = sqrt( pow(yCurrent-yt,2.0) + pow(xCurrent-xt,2.0) );
    if( distance < distanceMin ){
      distanceMin = distance;
      
      freneState.ts = tst;
      freneState.s = st;
      freneState.dts = dtst;
      freneState.ddts = ddtst;
      freneState.dddts = dddtst;
      freneState.td = tdt;
      freneState.d = dt;
      freneState.dtd = dtdt;
      freneState.ddtd = ddtdt;
      freneState.dddtd = dddtdt;
      
    }    
  }
  
}



void Frene::filterByOptimalityCostFromCartesianTrajectoryArray( std::vector< std::vector< std::vector<double> > >& trajectoryArray, 				  
								std::vector< std::vector< std::vector<double> > >& optimalTrajectoryArray, 
								double kjlong, double ktlong, double kplong, 
								double kjlat, double ktlat, double kplat, 				     
								double klong, double klat,
								int freneMode,
								double desiredSpeed, double desiredLatPosition, //for velocity keeping
								double desiredLongPosition  //for following, stopping
				    )			      
{
  double totalCostMin = 1e+14;
  std::vector< std::vector< std::vector< double > > >::iterator itrOptimal;//2d matrix
  
  for( std::vector< std::vector< std::vector<double> > >::iterator itr0 = trajectoryArray.begin(); itr0 != trajectoryArray.end(); itr0++ ){//itr0->2d matrix, itr1->1d vector
    //compute cost at the trajectory end point
    std::vector< double > itr1 = itr0->back();
    
    FreneState freneStateEnd;
    if( itr1.size() < FreneState::size() ){
      std::cout << "Merged trajectory size is incorrect" << endl;
      return;
    }

    freneStateEnd.ts    = itr1[8];
    freneStateEnd.s     = itr1[9];
    freneStateEnd.dts   = itr1[10];
    freneStateEnd.ddts  = itr1[11];
    freneStateEnd.dddts = itr1[12];	
    freneStateEnd.sumdddts = itr1[13];
    
    freneStateEnd.td    = itr1[14];
    freneStateEnd.d     = itr1[15];
    freneStateEnd.dtd   = itr1[16];
    freneStateEnd.ddtd  = itr1[17];
    freneStateEnd.dddtd = itr1[18];
    freneStateEnd.sumdddtd = itr1[19];
    
    double costLong = 0;
    switch( freneMode ){
    case 1:case 2: //following, stopping
      costLong = kjlong * freneStateEnd.sumdddts + ktlong * freneStateEnd.ts + kplong * fabs(freneStateEnd.s - desiredLongPosition );
      break;
    default://0, velocity keeping
      costLong = kjlong * freneStateEnd.sumdddts + ktlong * freneStateEnd.ts + kplong * fabs(freneStateEnd.dts - desiredSpeed ); 
      break;
    }
    double costLat  = kjlat  * freneStateEnd.sumdddtd + ktlat  * freneStateEnd.td + kplat  * fabs(freneStateEnd.d - desiredLatPosition );
    double totalCost = klong*costLong + klat*costLat;
    
    if( totalCost < totalCostMin ){
      totalCostMin = totalCost;
      itrOptimal = itr0;
    }
  }
    
  optimalTrajectoryArray.clear();
  optimalTrajectoryArray.push_back(*itrOptimal);  
}


void Frene::addDeadZone( std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 				  
			std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
			double sDeadZoneLength    )			      
{
  trajectoryArrayOut.clear(); 
  
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){        
    std::vector< std::vector<double> > tmp;
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){
      double x=(*itr2)[0];
      double y=(*itr2)[1];
      double t=(*itr2)[2];
      double theta=(*itr2)[3];
      double dtTheta=(*itr2)[4];
      double vx=(*itr2)[5];      
      double ax=(*itr2)[6];      
      double kx=(*itr2)[7];      
      
      double ts=(*itr2)[8];      
      double s=(*itr2)[9];            
      double dts=(*itr2)[10];                  
      double ddts=(*itr2)[11];                  
      double dddts=(*itr2)[12];                        
      double sumdddts=(*itr2)[13];                         
      
      double td=(*itr2)[14];      
      double d=(*itr2)[15];            
      double dtd=(*itr2)[16];                  
      double ddtd=(*itr2)[17];                  
      double dddtd=(*itr2)[18];                        
      double sumdddtd=(*itr2)[19];                         
      
      double centrifugalForce=(*itr2)[20];                               
      
      if( s > sDeadZoneLength ){
	vx = 0;
	dddts = 0;
      }
      
      std::vector<double> point;
      point.push_back(x);
      point.push_back(y);
      point.push_back(t);
      point.push_back(theta);
      point.push_back(dtTheta);
      point.push_back(vx);
      point.push_back(ax);
      point.push_back(kx);
      point.push_back(ts);
      point.push_back(s);
      point.push_back(dts);
      point.push_back(ddts);
      point.push_back(dddts);
      point.push_back(sumdddts);
      point.push_back(td);
      point.push_back(d);
      point.push_back(dtd);
      point.push_back(ddtd);
      point.push_back(dddtd);
      point.push_back(sumdddtd);
      point.push_back(centrifugalForce);

      tmp.push_back(point);
    }    
    trajectoryArrayOut.push_back(tmp);          
  }    





    
}




//find the path that has the largest deceleration
void Frene::filterByFindMostConservativeOne( std::vector< std::vector< std::vector<double> > >& trajectoryArray, 				  
					     std::vector< std::vector< std::vector<double> > >& optimalTrajectoryArray
					     )			      
{
  double totalCostMin = 1e+14;
  std::vector< std::vector< std::vector< double > > >::iterator itrOptimal;//2d matrix
  
  for( std::vector< std::vector< std::vector<double> > >::iterator itr0 = trajectoryArray.begin(); itr0 != trajectoryArray.end(); itr0++ ){//itr0->2d matrix, itr1->1d vector
    //compute cost at the trajectory at start point
    std::vector< double > itr1 = *(itr0->begin());
    std::vector< double > itr2 = itr0->back();
    
    FreneState freneStateStart;
    FreneState freneStateEnd;
    if( itr1.size() < FreneState::size() ){
      std::cout << "Merged trajectory size is incorrect" << endl;
      return;
    }

    freneStateStart.ts    = itr1[8];
    freneStateStart.s     = itr1[9];
    freneStateStart.dts   = itr1[10];
    freneStateStart.ddts  = itr1[11];
    freneStateStart.dddts = itr1[12];	
    freneStateStart.sumdddts = itr1[13];
    
    freneStateStart.td    = itr1[14];
    freneStateStart.d     = itr1[15];
    freneStateStart.dtd   = itr1[16];
    freneStateStart.ddtd  = itr1[17];
    freneStateStart.dddtd = itr1[18];
    freneStateStart.sumdddtd = itr1[19];

    freneStateEnd.ts    = itr2[8];
    freneStateEnd.s     = itr2[9];
    freneStateEnd.dts   = itr2[10];
    freneStateEnd.ddts  = itr2[11];
    freneStateEnd.dddts = itr2[12];	
    freneStateEnd.sumdddts = itr2[13];
    
    freneStateEnd.td    = itr2[14];
    freneStateEnd.d     = itr2[15];
    freneStateEnd.dtd   = itr2[16];
    freneStateEnd.ddtd  = itr2[17];
    freneStateEnd.dddtd = itr2[18];
    freneStateEnd.sumdddtd = itr2[19];
    
    double totalCost = freneStateStart.dddts;
    
    if( totalCost < totalCostMin ){
      totalCostMin = totalCost;
      itrOptimal = itr0;
    }
  }
    
  optimalTrajectoryArray.clear();
  optimalTrajectoryArray.push_back(*itrOptimal);  
}



void Frene::computeOptimalTrajectory( std::vector< std::vector< std::vector<double> > >& trajectoryArray, 
				      std::vector< std::vector< std::vector<double> > >& paramArray, 
				      std::vector< std::vector< std::vector<double> > >& optimalTrajectoryArray, 
				      std::vector< std::vector< std::vector<double> > >& optimalParamArray, 
				      double kjlong, double ktlong, double kplong, 
				      double kjlat, double ktlat, double kplat, 				     
				      double klong, double klat,
				      double desiredSpeed, double desiredLatPosition
				      )			      
{
  double totalCostMin = 1e+14;
  std::vector< std::vector< std::vector< double > > >::iterator itrOptimal;//2d matrix
  std::vector< std::vector< std::vector< double > > >::iterator itrOptimalParam;//2d matrix

  //iterator for param
  std::vector< std::vector< std::vector< double > > >::iterator itrParam = paramArray.begin();
  
  for( std::vector< std::vector< std::vector<double> > >::iterator itr0 = trajectoryArray.begin(); itr0 != trajectoryArray.end(); itr0++ ){//itr0->2d matrix, itr1->1d vector
    //compute cost at the trajectory end point
    std::vector< double > itr1 = itr0->back();
    
    FreneState freneStateEnd;
    if( itr1.size() < FreneState::size() ){
      std::cout << "Merged trajectory size is incorrect" << endl;
      return;
    }
    freneStateEnd.ts    = itr1[0];
    freneStateEnd.s     = itr1[1];
    freneStateEnd.dts   = itr1[2];
    freneStateEnd.ddts  = itr1[3];
    freneStateEnd.dddts = itr1[4];	
    freneStateEnd.sumdddts = itr1[5];

    freneStateEnd.td    = itr1[6];
    freneStateEnd.d     = itr1[7];
    freneStateEnd.dtd   = itr1[8];
    freneStateEnd.ddtd  = itr1[9];
    freneStateEnd.dddtd = itr1[10];
    freneStateEnd.sumdddtd = itr1[11];
    
    double costLong = kjlong * freneStateEnd.sumdddts + ktlong * freneStateEnd.ts + kplong * fabs(freneStateEnd.dts - desiredSpeed );
    double costLat  = kjlat  * freneStateEnd.sumdddtd + ktlat  * freneStateEnd.td + kplat  * fabs(freneStateEnd.d - desiredLatPosition );
    double totalCost = klong*costLong + klat*costLat;
    
    if( totalCost < totalCostMin ){
      totalCostMin = totalCost;
      itrOptimal = itr0;
      itrOptimalParam = itrParam;
    }
        
    itrParam++;
  }
    
  //LaneList::print(*itrOptimal);
  
  optimalTrajectoryArray.clear();
  optimalTrajectoryArray.push_back(*itrOptimal);  
  
  optimalParamArray.clear();
  optimalParamArray.push_back(*itrOptimalParam);  
  
  //std::cout << "Optimal" << std::endl;
  //TrajectoryArray::print(optimalTrajectoryArray);
  //TrajectoryArray::print(optimalParamArray);
  //std::cout << "desired position & speed " << desiredLatPosition << " " << desiredSpeed << std::endl;
}

void Frene::computeTrajectoryZero( std::vector< std::vector< std::vector<double> > >& previousOptimalTrajectory, 
				   std::vector< std::vector< std::vector<double> > >& trajectoryZero,
				   double trajectoryLength,
				   int numOfPoints )
{
  trajectoryZero.clear();
  
  if( previousOptimalTrajectory.size() <= 0 ){
    return;
  }
  double x0 = previousOptimalTrajectory[0][0][0];
  double y0 = previousOptimalTrajectory[0][0][1];
  double theta0 = previousOptimalTrajectory[0][0][3]; //yaw angle
  
  Eigen::VectorXd p0(2); p0 << x0, y0;
  Eigen::VectorXd d0(2); d0 << cos(theta0), sin(theta0);
  //double    d = (p - rr).dot( nr );
  //double    distance = (p - rr).norm();  
  //double    distanceAbs = fabs(distance);

  Eigen::VectorXd p(2);   
  double distance = 0;
  double delta = trajectoryLength / (double)(numOfPoints-1);

  std::vector< double > point(21);
  std::vector< std::vector<double> > tmp;
  tmp.clear();
  
  for( int itr=0; itr < numOfPoints; itr++ ){    
    distance = delta*itr;
    p = p0 + distance * d0;
    
    point[0] = p(0);//x
    point[1] = p(1);//y
    point[2] = itr;//t
    point[3] = theta0;//theta
    point[4] = 0;//dtTheta
    point[5] = 0;//vx
    point[6] = 0;//ax    
    point[7] = 0;//kx
    point[8] = itr;//ts
    point[9] = 0;//s
    point[10] = 0;//dts
    point[11] = 0;//ddts
    point[12] = 0;//dddts
    point[13] = 0;//sumdddts
    point[14] = itr;//td
    point[15] = 0;//d
    point[16] = 0;//dtd
    point[17] = 0;//ddtd
    point[18] = 0;//dddtd
    point[19] = 0;//sumdddtd
    point[20] = 0;//centrifugalForce
    
    tmp.push_back(point);    
  }

  trajectoryZero.push_back(tmp);
  
}

void Frene::computeTrajectorySmoothStopping( std::vector< std::vector< std::vector<double> > >& previousOptimalTrajectory, 
					 std::vector< std::vector< std::vector<double> > >& trajectoryZero,
					 double trajectoryLength,
					 double smoothnessFactor,
					 int numOfPoints )
{
  trajectoryZero.clear();
  
  if( previousOptimalTrajectory.size() <= 0 ){
    return;
  }
  double x0 = previousOptimalTrajectory[0][0][0];
  double y0 = previousOptimalTrajectory[0][0][1];
  double theta0 = previousOptimalTrajectory[0][0][3]; //yaw angle
  double vx0 = previousOptimalTrajectory[0][0][5];
  
  Eigen::VectorXd p0(2); p0 << x0, y0;
  Eigen::VectorXd d0(2); d0 << cos(theta0), sin(theta0);
  //double    d = (p - rr).dot( nr );
  //double    distance = (p - rr).norm();  
  //double    distanceAbs = fabs(distance);

  Eigen::VectorXd p(2);   
  double distance = 0;
  double distanceNext = 0;
  double delta = trajectoryLength / (double)(numOfPoints-1);
  double vx = 0;
  
  std::vector< double > point(21);
  std::vector< std::vector<double> > tmp;
  tmp.clear();

  for( int itr=0; itr < numOfPoints; itr++ ){    
    distance = delta*itr;
    distanceNext = delta*(itr+1);
    p = p0 + distance * d0;
    vx = vx0*exp(-smoothnessFactor*distanceNext);
    
    point[0] = p(0);//x
    point[1] = p(1);//y
    point[2] = itr;//t
    point[3] = theta0;//theta
    point[4] = 0;//dtTheta
    point[5] = vx;//vx
    point[6] = 0;//ax    
    point[7] = 0;//kx
    point[8] = itr;//ts
    point[9] = 0;//s
    point[10] = 0;//dts
    point[11] = 0;//ddts
    point[12] = 0;//dddts
    point[13] = 0;//sumdddts
    point[14] = itr;//td
    point[15] = 0;//d
    point[16] = 0;//dtd
    point[17] = 0;//ddtd
    point[18] = 0;//dddtd
    point[19] = 0;//sumdddtd
    point[20] = 0;//centrifugalForce
    
    tmp.push_back(point);    

  }

  trajectoryZero.push_back(tmp);
  
}


void Frene::filterBySafety(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
			   std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
			   double roadMarginRight,  
			   double roadMarginLeft,     
			   double maxCentrifugalForce,
			   double maxAcceleration,
			   double maxCurvature  	
			   )
{  

  trajectoryArrayOut.clear();
  double minValue = 1e+14;//to find second best solution
  std::vector< std::vector< double > >::iterator itrMin;
  
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){        
    std::vector< std::vector<double> > tmp;
    bool isSafe = true;
    
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){
      double x=(*itr2)[0];
      double y=(*itr2)[1];
      double t=(*itr2)[2];
      double theta=(*itr2)[3];
      double dtTheta=(*itr2)[4];
      double vx=(*itr2)[5];      
      double ax=(*itr2)[6];      
      double kx=(*itr2)[7];      
      
      double ts=(*itr2)[8];      
      double s=(*itr2)[9];            
      double dts=(*itr2)[10];                  
      double ddts=(*itr2)[11];                  
      double dddts=(*itr2)[12];                        
      double sumdddts=(*itr2)[13];                         
      
      double td=(*itr2)[14];      
      double d=(*itr2)[15];            
      double dtd=(*itr2)[16];                  
      double ddtd=(*itr2)[17];                  
      double dddtd=(*itr2)[18];                        
      double sumdddtd=(*itr2)[19];                         
      
      double centrifugalForce=(*itr2)[20];                               
      
      if( ax < minValue ){
      	minValue = ax;
	itrMin = itr2;
      }     
      //if( fabs(d) < minValue ){
      //minValue = fabs(d);
      //	itrMin = itr2;
      //}      
      
      if( fabs(centrifugalForce) > maxCentrifugalForce ){
	//std::cout << "centrifugalForce out of range " << fabs(centrifugalForce) << " " << maxCentrifugalForce << std::endl;
	isSafe = false;
	break;
      }
      if( ax > maxAcceleration ){
	//std::cout << "acceleration out of range " << ax << " " << maxAcceleration << std::endl;
	isSafe = false;
	break;
      }
      if( fabs(kx) > maxCurvature ){
	//std::cout << "maxCurvature out of range " << fabs(kx) << " " << maxCurvature << std::endl;
	isSafe = false;
	break;
      }
      if( d > roadMarginLeft || d < -roadMarginRight ){//d is positive in left direction
	//std::cout << "roadMargin out of range " << d << " " << roadMarginLeft << " " << roadMarginRight << std::endl;
	isSafe = false;
	break;
      }

      tmp.push_back(*itr2);
    }
    
    if( isSafe ){     
      trajectoryArrayOut.push_back(tmp);          
    }
    else{
      //filter out
    }
  }    
  
  //if( trajectoryArrayOut.size() <=0 ){
  //std::vector< std::vector<double> > tmpMin;
  //tmpMin.push_back(*itrMin);      
  //trajectoryArrayOut.push_back(tmpMin);     
  //}
  
}

grid_map::Position applyTransform(grid_map::Position in, Eigen::Vector2d trans, double yaw){
  Eigen::Rotation2Dd rot(-yaw);
  return rot*(in-trans);
  
}


/* 
//Original Function by Alvin (not working)
void Frene::filterByCollision(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
			      std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
			      grid_map::GridMap map,
			      double Length,
			      double Width,
			      Eigen::Vector2d currentPos,
			      double yaw
			      )
{  
 
  trajectoryArrayOut.clear();
  // return;

  //ROS_ERROR("Debug1" );//,ns.c_str());
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){        
    std::vector< std::vector<double> > tmp;
    bool isSafe = true;
    
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){
      double x=(*itr2)[0];
      double y=(*itr2)[1];
      double theta=(*itr2)[3];
      tf::Vector3 left=tf::Vector3(Length/2,Width/2,0);
      tf::Vector3 right=tf::Vector3(Length/2,-Width/2,0);
      tf::Vector3 center=tf::Vector3(x,y,0);
      tf::Quaternion orientation=tf::createQuaternionFromYaw(theta);
      tf::Vector3 rotLeft=tf::quatRotate(orientation,left)+center;
      tf::Vector3 rotRight=tf::quatRotate(orientation,right)+center;

      grid_map::Position gridLeft(rotLeft.getX(),rotLeft.getY());
      grid_map::Position gridRight(rotRight.getX(),rotRight.getY());
      grid_map::Position rotGridLeft=applyTransform(gridLeft,currentPos,yaw);
      grid_map::Position rotGridRight=applyTransform(gridRight,currentPos,yaw);
      
      grid_map::Index idxleft,idxright;
      if(!map.getIndex(rotGridLeft,idxleft)||!map.getIndex(rotGridRight,idxright)){
	isSafe=false;
	ROS_ERROR("Not Safe 1" );
	break;
      }
      //Only check the front of the vehicle for collision - it can only move forwards
      for(grid_map::LineIterator iterator(map,idxright,idxleft);!iterator.isPastEnd(); ++iterator){
	if(map.at("traversability",*iterator)==1.0){
	  isSafe=false;
	  ROS_ERROR("Not Safe 2" );
	  break;
	}
      }
      tmp.push_back(*itr2);
    }

    //ROS_ERROR("Debug2" );//,ns.c_str());
    
    if( isSafe ){     
      trajectoryArrayOut.push_back(tmp);          
    }

  }    
 
}
*/

/*
  std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
			      std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
			      grid_map::GridMap map,
			      double Length,
			      double Width,
			      Eigen::Vector2d currentPos,
			      double yaw


Frene::Collision(geometry_msgs::PoseStamped currentPose)
{   
  this->currentPose=currentPose;
}
*/
void Frene::filterByCollision(
			      std::vector< std::vector< std::vector< double > > >& trajectoryArrayCartesianListIn, 
			      std::vector< std::vector< std::vector< double > > >& trajectoryArrayCartesianListOut, 				    
			      grid_map::GridMap map,
			      double Length,
			      double Width,
			      Eigen::Vector2d curPos,
			      double yaw
			      )
{

  geometry_msgs::PoseStamped currentPose;
  currentPose.pose.position.x = curPos[0];
  currentPose.pose.position.y = curPos[1];

  trajectoryArrayCartesianListOut.clear();
  
  int count=0;
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayCartesianListIn.begin(); itr1 != trajectoryArrayCartesianListIn.end(); itr1++ ){//do for each trajectories

    std::vector< std::vector<double> > tmp;
    bool isSafe = true;
    
    //Infer path from 2 consecute points in the path
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end()-1; itr2++ ){//do for each points in the trajectory
      double x=(*itr2)[0];
      double y=(*itr2)[1];
      double theta=(*itr2)[3];
      //double vx=(*itr2)[5];
      
      tf::Vector3 point(x,y,0);
      tf::Quaternion orientation=tf::createQuaternionFromYaw(theta);
      tf::Vector3 footprint[4];
      footprint[0]=tf::Vector3(Length/2,Width/2,0);
      footprint[1]=tf::Vector3(-Length/2,Width/2,0);
      footprint[2]=tf::Vector3(Length/2,-Width/2,0);
      footprint[3]=tf::Vector3(-Length/2,-Width/2,0);
      Eigen::Vector2d pos[4];
      grid_map::Index idx[4];
      
      for(int i=0;i<=3;i++){
	tf::Vector3 preCorner=tf::quatRotate(orientation, footprint[i])+point;
	tf::Vector3 corner=preCorner;
	pos[i]<<corner.getX(),corner.getY();
	pos[i]=applyTransform2(pos[i], currentPose.pose.position.x,
			       currentPose.pose.position.y, 
			       //tf::getYaw(currentPose.pose.orientation)
			       yaw
			       );
	map.getIndex(pos[i],idx[i]);
      }
      
      for(int i=0; i<4;i++){
	for(grid_map::LineIterator iterator(map,idx[i],idx[(i+1)%4]);!iterator.isPastEnd(); ++iterator){
	  if(map.at("traversability",*iterator)==1.0){
	    isSafe=false;
	    break;
	  }
	}
	if( !isSafe )
	  break;
      }
      // grid_map::Polygon polygon;
      // polygon.setFrameId(map.getFrameId());
      // polygon.addVertex(pos[0]);
      // polygon.addVertex(pos[1]);
      // polygon.addVertex(pos[2]);
      // polygon.addVertex(pos[3]);
      // for(grid_map::PolygonIterator iterator(map,polygon); !iterator.isPastEnd(); ++iterator){
      // 	if(map.at("traversability",*iterator)==1.0){
      // 	  isSafe=false;
      // 	  break;
      // 	}
      // }
      if( !isSafe ){
	count++;
	break;	
      }
      
      tmp.push_back(*itr2);      
    }
    
    if( isSafe ){     
      trajectoryArrayCartesianListOut.push_back(tmp);
      tmp.clear();
    }

  }
 
  //std::cerr<<"Collided Trajectories: "<<count<<" of input: "<<trajectoryArrayCartesianListIn.size()<<"   out: "<< trajectoryArrayCartesianListOut.size()  << std::endl; 
}


			      

void Frene::multiplyAmplitude(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
			      std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
			      double amplitude
			   )
{  
  trajectoryArrayOut.clear();
  
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){        
    std::vector< std::vector<double> > tmp;
    for( std::vector< std::vector< double > >::iterator itr2=itr1->begin(); itr2 != itr1->end(); itr2++ ){
      double x=(*itr2)[0];
      double y=(*itr2)[1];
      double t=(*itr2)[2];
      double theta=(*itr2)[3];
      double dtTheta=(*itr2)[4];
      double vx=amplitude*(*itr2)[5];      
      double ax=amplitude*(*itr2)[6];      
      double kx=amplitude*(*itr2)[7];      
      
      double ts=(*itr2)[8];      
      double s=(*itr2)[9];            
      double dts=amplitude*(*itr2)[10];                  
      double ddts=amplitude*(*itr2)[11];                  
      double dddts=amplitude*(*itr2)[12];                        
      double sumdddts=amplitude*(*itr2)[13];                         
      
      double td=(*itr2)[14];      
      double d=(*itr2)[15];            
      double dtd=amplitude*(*itr2)[16];                  
      double ddtd=amplitude*(*itr2)[17];                  
      double dddtd=amplitude*(*itr2)[18];                        
      double sumdddtd=amplitude*(*itr2)[19];                         
      
      double centrifugalForce=amplitude*(*itr2)[20];                               
      
      std::vector<double> point;
      point.push_back(x);
      point.push_back(y);
      point.push_back(t);
      point.push_back(theta);
      point.push_back(dtTheta);
      point.push_back(vx);
      point.push_back(ax);
      point.push_back(kx);
      point.push_back(ts);
      point.push_back(s);
      point.push_back(dts);
      point.push_back(ddts);
      point.push_back(dddts);
      point.push_back(sumdddts);
      point.push_back(td);
      point.push_back(d);
      point.push_back(dtd);
      point.push_back(ddtd);
      point.push_back(dddtd);
      point.push_back(sumdddtd);
      point.push_back(centrifugalForce);

      tmp.push_back(point);
    }    
    trajectoryArrayOut.push_back(tmp);          
  }    
    
}






void Frene::filterByDesiredLatPosition(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
				       std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
				       double desiredLatPosition
				       )
{   
  trajectoryArrayOut.clear();
  
  //now find min value at end
  double minDiff = 1e+14;
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    
    //compute cost at the trajectory end point
    std::vector< double > itr2 = itr1->back();    
    double d=itr2[15]; 
    
    double diff = fabs(desiredLatPosition-d);
    if( diff < minDiff ){
      minDiff = diff;
    }    
  }
  
  //filter out trajectories > minDiff
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    

    std::vector< double > itr2 = itr1->back();    
    double d=itr2[15];
    
    double diff = fabs(desiredLatPosition-d);
    double epsValue = 1e-04;
    if( diff < minDiff || fabs(diff-minDiff) < epsValue ){
      trajectoryArrayOut.push_back(*itr1);    
    }
  }
}

void Frene::filterByDesiredLongPosition(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
				       std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
				       double desiredLongPosition
				       )
{   
  trajectoryArrayOut.clear();
  
  //now find min value at end
  double minDiff = 1e+14;
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    
    //compute cost at the trajectory end point
    std::vector< double > itr2 = itr1->back();    
    double s=itr2[9]; 
    
    double diff = fabs(desiredLongPosition-s);
    if( diff < minDiff ){
      minDiff = diff;
    }    
  }
  
  //filter out trajectories > minDiff
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    

    std::vector< double > itr2 = itr1->back();    
    double s=itr2[9];
    
    double diff = fabs(desiredLongPosition-s);
    double epsValue = 1e-04;
    if( diff < minDiff || fabs(diff-minDiff) < epsValue ){
      trajectoryArrayOut.push_back(*itr1);    
    }
  }
}



void Frene::filterByDesiredSpeed(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
				 std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut,
				 double desiredSpeed
				 )
{   
  trajectoryArrayOut.clear();
  
  //now find min value at end
  double minDiff = 1e+14;
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    
    //compute cost at the trajectory end point
    std::vector< double > itr2 = itr1->back();    
    double vx=itr2[5]; 
    double diff = fabs(desiredSpeed-vx);
    if( diff < minDiff ){
      minDiff = diff;
    }
  }
  
  //filter out trajectories > minDiff
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){        
    std::vector< double > itr2 = itr1->back();    
    double vx=itr2[5];
    
    double diff = fabs(desiredSpeed-vx);
    double epsValue = 1e-04;
    if( diff < minDiff || fabs(diff-minDiff) < epsValue ){
      trajectoryArrayOut.push_back(*itr1);    
    }

  }
}

void Frene::filterByComfort(std::vector< std::vector< std::vector<double> > >& trajectoryArrayIn, 
			    std::vector< std::vector< std::vector<double> > >& trajectoryArrayOut			    
			    )
{   
  trajectoryArrayOut.clear();
  
  //now find min value at end
  double minCost = 1e+14;
  std::vector< std::vector< std::vector< double > > >::iterator minItr;
  
  double minTs = 0;
  for( std::vector< std::vector< std::vector< double > > >::iterator itr1=trajectoryArrayIn.begin(); itr1 != trajectoryArrayIn.end(); itr1++ ){    
      //compute cost at the trajectory end point
    std::vector< double > itr2 = itr1->back();    
    double ts=itr2[8]; 
    double sumdddts=itr2[13]; 
    double sumdddtd=itr2[19];
    double cost = sumdddtd + sumdddts;// + sumdddtd;
    //cost = cost / (double)(itr1->size());
    cost = cost / (double)ts / (double)(itr1->size());
    
    if( cost < minCost ){
      minCost = cost;
      minItr = itr1;
      minTs = ts;
    }    
  }
  trajectoryArrayOut.push_back(*minItr);
  
  //std::cout << minCost << " " << minTs << std::endl;

}


void Frene::setAsInvalidTrajectory( std::vector< std::vector< std::vector< double > > >& path )
{
  if( path.size() <= 0 ){
    std::cout << "Frene::path size is invalid." << std::endl;
    return;
  }
  if( path[0].size() <= 0 ){
    std::cout << "Frene::path size is invalid." << std::endl;
    return;
  }
  if( path[0][0].size() <= 2 ){
    std::cout << "Frene::path size is invalid." << std::endl;
    return;
  }

  //set negative time value to the first trajectory element 
  path[0][0][2] = -1;
}


bool Frene::isInvalidTrajectory( std::vector< std::vector< std::vector< double > > >& path )
{
  if( path.size() <= 0 ){
    return true;
  }
  if( path[0].size() <= 0 ){
    return true;
  }
  if( path[0][0].size() <= 2 ){
    return true;
  }
  

  //set negative time value to the first trajectory element 
  bool isIllegal = false;

  if( path[0][0][2] < 0 ){
    isIllegal = true;
  }
  
  return isIllegal;
}





/*

void Frene::interpXYListFromSpline( //spline_struct& splineX, tk::spline& splineY,
				   std::vector< std::vector<double> >& xyInterpList, double interpTick, double totalArcLength, spline_struct& _splineX, spline_struct& _splineY )
{
  xyInterpList.clear();
  
  for( double ss=0; ss < totalArcLength; ss+= interpTick ){
    double valueX=_splineX(ss); 
    double valueY=_splineY(ss); 
    
    double valuedX = _splineX.deriv(1, ss);
    double valuedY = _splineY.deriv(1, ss);
    
    double valueddX = _splineX.deriv(2, ss);
    double valueddY = _splineY.deriv(2, ss);
    
    double valuedddX = _splineX.deriv(3, ss);
    double valuedddY = _splineY.deriv(3, ss);
    
    std::vector< double > xx;
    xx.push_back(valueX);
    xx.push_back(valueY);
    xx.push_back(ss);//LaneList optional invariant values
    xx.push_back(valuedX);
    xx.push_back(valuedY);
    xx.push_back(valueddX);
    xx.push_back(valueddY);
    xx.push_back(valuedddX);
    xx.push_back(valuedddY);
    
    xyInterpList.push_back(xx);    
    //ROS_INFO_STREAM( "sxy: " << ss << " " << valueX << " " << valueY  );
  }  
}
*/