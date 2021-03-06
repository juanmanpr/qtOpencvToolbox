/*
    @file: computervisioninterface.cpp
    @license: GNU General Public License
    @author: Juan Manuel Perez Rua
    @note: Code written for th practical module of
    Visual Perception at the Université de Bourgogne
*/

#include "computervisioninterface.h"
#include <vector>
#include <QtTest>

#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/features2d.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/stitching/stitcher.hpp"
#include "opencv2/contrib/contrib.hpp"
#include "robustmatcher.h"
#include "Distance.h"
#include "MultiCameraPnP.h"

/* Interface Global Variables */

QImage qImage1;
QImage qImage1Histogram;
QImage qProccessedImage;
double FM[3][3]={{0,0,0},{0,0,0},{0,0,1}};
double HG[3][3]={{0,0,0},{0,0,0},{0,0,1}};
double EC[3][3]={{0,0,0},{0,0,0},{0,0,1}};
std::vector<cv::Point3d> structure;
std::vector<cv::Vec3b> colors;

ComputerVisionInterface::ComputerVisionInterface(){
    this->addSaltPepperNoise=false;
    this->addGaussianNoise=false;
    this->noisePower=50;/*min=0, max=99*/
    this->noiseStdDev=50;/*min=0, max=99*/
    this->convertToGray=false;
    this->updateHistogram=false;
    this->equalizeHistogram=false;
    this->rgbToHls=false;
    this->rgbToHsv=false;
    this->rgbToXyz=false;
    this->rgbToYcbcr=false;
    this->rgbToLab=false;
    this->rgbToLuv=false;
    this->logoActivated=false;
    this->logoFilename="";
    this->transparency=50;
    this->workingOnCam=false;
    this->workingOnFrame=false;
    this->workingOnVideo=false;
    this->setLoopLock(true);
    this->endVideo=false;
    this->morpho="NONE";
    this->filter="NONE";
    this->hough="NONE";
    this->shape="NONE";
    this->feature="NONE";
    this->stereo = "NONE";
    this->fundamentalMethod="NONE";
    this->featureParam=50;
    this->filterParam=50;
    this->morphoSize=3;
    this->canny=false;
    this->cannyParam1=50;
    this->cannyParam2=50;
    this->conObjs = false;
    this->threshold=50;
    this->contours=false;
    this->bmview1=false;
    this->bmview2=false;
    this->calibrate=false;
    this->calibrated=false;
    this->showmview=1;
    this->numImagesCalibration=30;
    this->calibImageIndex=0;
    this->addImageToStitch=false;
    this->stitch=false;
    this->doSfm=false;
    this->addImageToSFM=false;
    this->addImageToSFMFF=false;
    this->addImageToStitchFF=false;
}

ComputerVisionInterface::~ComputerVisionInterface(){
}

void ComputerVisionInterface::process(){
    computerVisionMachine();
    emit finished();
}

/* MAIN STATE MACHINE */
void ComputerVisionInterface::computerVisionMachine(void){
    cv::Mat frame;
    cv::Mat proccessedImage;

    if (this->workingOnCam){
        this->endVideo=false;
        setVideoCapturer1(CV_CAP_ANY);
    }

    if (this->workingOnVideo){
        this->endVideo=false;
        this->capture1.open(this->videoFilename);
        //std::cout<<this->videoFilename<<std::endl;
    }

    while(this->loopLocked){
        if (this->workingOnCam){
            /* Capture Image from camera1 and convert to QImage */
            //capture1 >> frame is also possible

            if (!capture1.grab() || !capture1.retrieve(frame) ){
                std::cout<<"Video Stopped"<<std::endl;
                this->endVideo = true;
            }
            qImage1 = Mat2QImage(frame);
            proccessedImage = frame.clone();
        }
        if (this->workingOnVideo){
            if (!capture1.grab() || !capture1.retrieve(frame) ){
                std::cout<<"Video Stopped"<<std::endl;
                this->endVideo = true;
            }
            qImage1 = Mat2QImage(frame);
            proccessedImage = frame.clone();
        }
        if (this->workingOnFrame){
            frame = cv::imread(this->frameFilename, CV_LOAD_IMAGE_COLOR);
            qImage1 = Mat2QImage(frame);
            proccessedImage = frame.clone();
            QTest::qSleep(100);
        }

        /* Start asking about proccessing options */
        if (this->bmview1){
            frame.copyTo(this->mview1);
            this->bmview1=false;
        }

        if (this->bmview2){
            frame.copyTo(this->mview2);
            this->bmview2=false;
        }

        if (this->logoActivated){
            cv::Mat logo, rlogo;
            logo = cv::imread(this->logoFilename, CV_LOAD_IMAGE_COLOR);
            cv::resize(logo, rlogo, cv::Size(proccessedImage.cols/5, proccessedImage.rows/5),CV_INTER_LINEAR);
            cv::Rect roi = cv::Rect( (proccessedImage.cols-rlogo.cols)*(this->xlogo/100),
                                     (proccessedImage.rows-rlogo.rows)*(this->ylogo/100),
                                    rlogo.cols,
                                    rlogo.rows );
            cv::Mat subView = proccessedImage(roi);
            cv::addWeighted(rlogo,1-(double)this->transparency/100,subView,(double)this->transparency/100,0,subView);
            //rlogo.copyTo(subView);
        }

        if (this->addSaltPepperNoise){
            cv::Mat saltedMatrix = cv::Mat::zeros(proccessedImage.rows, proccessedImage.cols, CV_8U);
            cv::randu(saltedMatrix, 0, 255);
            cv::Mat black = saltedMatrix < (127*double(noisePower)/100);
            cv::Mat white = saltedMatrix > (255-127*double(noisePower)/100);
            proccessedImage.setTo(255,white);
            proccessedImage.setTo(0,black);
        }
        if (this->addGaussianNoise){
            cv::Mat noisedMatrix = proccessedImage.clone();
            cv::randn(noisedMatrix,int(double(noisePower)/2),255*noiseStdDev/100);
            double maxVal1, maxVal2;
            cv::minMaxLoc(noisedMatrix, NULL, &maxVal1, NULL, NULL);
            cv::minMaxLoc(proccessedImage, NULL, &maxVal2, NULL, NULL);
            cv::addWeighted(noisedMatrix, 255/(maxVal1+maxVal2), proccessedImage, 255/(maxVal2+maxVal1), 0, proccessedImage);
        }
        if (this->convertToGray){
            cv::Mat gray;
            cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
            cv::cvtColor(gray,proccessedImage, CV_GRAY2BGR);
        }
        if (this->equalizeHistogram){
            cv::Mat gray, equalizedBuffer;
            cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
            cv::equalizeHist(gray, equalizedBuffer);
            cv::cvtColor(equalizedBuffer,proccessedImage, CV_GRAY2BGR);
        }
        if (this->rgbToHls){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2HLS);
        }
        if (this->rgbToHsv){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2HSV);
        }
        if (this->rgbToYcbcr){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2YCrCb);
        }
        if (this->rgbToXyz){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2XYZ);
        }
        if (this->rgbToLuv){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2Luv);
        }
        if (this->rgbToLab){
            cv::cvtColor(proccessedImage, proccessedImage, CV_BGR2Lab);
        }
        if (morpho.compare("NONE",Qt::CaseSensitive)!=0){
            if (morpho.compare("OPEN")==0){
                cv::morphologyEx(proccessedImage, proccessedImage, cv::MORPH_CLOSE, cv::Mat(this->morphoSize,this->morphoSize,CV_8U,cv::Scalar(1)));
            }else if (morpho.compare("CLOSE")==0){
                cv::morphologyEx(proccessedImage, proccessedImage, cv::MORPH_CLOSE, cv::Mat(this->morphoSize,this->morphoSize,CV_8U,cv::Scalar(1)));
            }else if (morpho.compare("DILATE")==0){
                cv::dilate(proccessedImage, proccessedImage,cv::Mat(this->morphoSize,this->morphoSize,CV_8U,cv::Scalar(1)));
            }else if (morpho.compare("ERODE")==0){
                cv::erode(proccessedImage, proccessedImage,cv::Mat(this->morphoSize,this->morphoSize,CV_8U,cv::Scalar(1)));
            }
        }
        if (filter.compare("NONE",Qt::CaseSensitive)!=0){
            if (filter.compare("BLUR")==0){
                cv::blur(proccessedImage, proccessedImage, cv::Size(50*this->filterParam/100+1,50*this->filterParam/100+1));
            }else if (filter.compare("SHARP")==0){
                cv::Mat image;
                int size = this->filterParam; if (size%2==0)size++;
                cv::GaussianBlur(proccessedImage, image, cv::Size(size,size),75);
                cv::addWeighted(proccessedImage, 1.5, image, -0.5, 0, proccessedImage);
            }else if (filter.compare("SOBEL")==0){
                int size=0;
                if (this->filterParam<33) size=3;
                else if (this->filterParam<66) size=5;
                else if (this->filterParam<100) size=7;
                cv::Sobel(proccessedImage, proccessedImage, proccessedImage.depth(),1,1, size );
            }else if (filter.compare("LAPLACIAN")==0){
                int size = 31*this->filterParam/100; if (size%2==0)size++;
                cv::Laplacian(proccessedImage, proccessedImage, proccessedImage.depth(),size);
            }
        }
        if (this->canny){
            cv::Canny(proccessedImage, proccessedImage, this->cannyParam1, this->cannyParam2);
            cv::cvtColor(proccessedImage,proccessedImage, CV_GRAY2BGR);
        }
        if (hough.compare("NONE",Qt::CaseSensitive)!=0){
            if (hough.compare("LINES")==0){
                cv::Mat gray, buffer;
                cv::Canny(proccessedImage, buffer, this->cannyParam1, this->cannyParam2);
                cv::cvtColor(buffer, buffer, CV_GRAY2BGR);
                std::vector<cv::Vec4i> lines;
                cv::cvtColor(buffer, gray, CV_BGR2GRAY);
                cv::HoughLinesP(gray, lines, 1, CV_PI/180, this->houghParam+1,30,5);
                for (unsigned int i=0; i<lines.size();i++){
                    cv::Vec4i li = lines[i];
                    cv::line(proccessedImage, cv::Point(li[0],li[1]),
                             cv::Point(li[2],li[3]), cv::Scalar(255,255,0),
                             3, CV_AA);
                }
            }else if (hough.compare("CIRCLES")==0){
                cv::Mat gray;
                if (proccessedImage.depth()==frame.depth())
                    cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                else
                    gray = proccessedImage;
                std::vector<cv::Vec3f> circles;
                cv::HoughCircles( gray, circles, CV_HOUGH_GRADIENT,1, this->houghParam+1,
                                  this->cannyParam1+1, this->cannyParam2+1, 0, 0 );

                for (unsigned int i=0; i<circles.size();i++){
                    cv::Point cen(cvRound(circles[i][0]),cvRound(circles[i][1]));
                    int rad = cvRound(circles[i][2]);
                    cv::circle( proccessedImage, cen, 3, cv::Scalar(0,0,255), -1, 8, 0 );
                    cv::circle( proccessedImage, cen, rad, cv::Scalar(255,0,0), 3, 8, 0 );
                }
            }
        }
        if (this->conObjs){
            cv::Mat gray;
            cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
            std::vector< std::vector<cv::Point> > contours;

            cv::threshold(gray, gray, 255*this->threshold/100, 255, CV_THRESH_BINARY);
            cv::cvtColor(gray,proccessedImage, CV_GRAY2BGR);
            cv::findContours(gray, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            cv::drawContours(proccessedImage, contours, -1/*Draw all contours*/, cv::Scalar(255,255,255),10);
        }
        if (this->contours){
            cv::Mat buffer;
            std::vector< std::vector<cv::Point> > contours;
            cv::cvtColor(proccessedImage, buffer, CV_BGR2GRAY);
            cv::threshold(buffer, buffer, 255*this->threshold/100, 255, CV_THRESH_BINARY);
            cv::findContours(buffer, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            cv::drawContours(proccessedImage, contours, -1/*Draw all contours*/, cv::Scalar(255,255,0),2);
        }
        if (shape.compare("NONE",Qt::CaseSensitive)!=0){
            if (shape.compare("BOX")==0){
                cv::Mat buffer;
                std::vector< std::vector<cv::Point> > contours;
                cv::cvtColor(proccessedImage, buffer, CV_BGR2GRAY);
                cv::threshold(buffer, buffer, 255*this->threshold/100, 255, CV_THRESH_BINARY);
                cv::findContours(buffer, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                for (unsigned int i=0; i<contours.size(); i++){
                    cv::Rect boxi = cv::boundingRect(cv::Mat(contours[i]));
                    cv::rectangle(proccessedImage, boxi, cv::Scalar(0,0,255),3);
                }
            }else if (shape.compare("CIRCLE")==0){
                cv::Mat buffer;
                std::vector< std::vector<cv::Point> > contours;
                cv::cvtColor(proccessedImage, buffer, CV_BGR2GRAY);
                cv::threshold(buffer, buffer, 255*this->threshold/100, 255, CV_THRESH_BINARY);
                cv::findContours(buffer, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                for (unsigned int i=0; i<contours.size(); i++){
                    float rad;
                    cv::Point2f cen;
                    cv::minEnclosingCircle(cv::Mat(contours[i]), cen, rad);
                    cv::circle(proccessedImage, cen, rad, cv::Scalar(0,0,255),3);
                }
            }else if (shape.compare("CENTER")==0){
                cv::Mat buffer;
                std::vector< std::vector<cv::Point> > contours;
                cv::cvtColor(proccessedImage, buffer, CV_BGR2GRAY);
                cv::threshold(buffer, buffer, 255*this->threshold/100, 255, CV_THRESH_BINARY);
                cv::findContours(buffer, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                for (unsigned int i=0; i<contours.size(); i++){
                    float rad;
                    cv::Point2f cen;
                    cv::minEnclosingCircle(cv::Mat(contours[i]), cen, rad);
                    cv::circle(proccessedImage, cen, 3, cv::Scalar(0,0,255),3);
                }
            }
        }

        if (feature.compare("NONE",Qt::CaseSensitive)!=0){
            if (feature.compare("MSER")==0){
                cv::Mat gray;
                std::vector< std::vector< cv::Point > > keys;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                cv::MSER mserDet;
                mserDet.operator ()(gray, keys, cv::Mat());
                cv::drawContours(proccessedImage, keys, -1/*Draw all contours*/, cv::Scalar(255,0,0),2);

            }else if (feature.compare("HARRIS")==0){
                // Detect Harris Corner
                cv::Mat gray, norm;
                int thresh=255*this->featureParam/100;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                cv::cornerHarris(gray,gray,100,3,0.04 /*Harris parameter*/);

                /// Normalizing
                cv::normalize( gray, norm, 0, 255, cv::NORM_MINMAX, CV_32FC1, cv::Mat() );

                /// Drawing a circle around corners
                for( int j = 0; j < norm.rows ; j++ ){
                    for( int i = 0; i < norm.cols; i++ ){
                      if( (int) norm.at<float>(j,i) > thresh ){
                        cv::circle( proccessedImage, cv::Point(i,j), 1,  cv::Scalar(255,255,0), 1);
                      }
                    }
                }
            }else if (feature.compare("HARRIS_NMS")==0){
                cv::Mat gray;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);

                std::vector< cv::Point > strong_corners;
                cv::goodFeaturesToTrack(gray, strong_corners, 100, this->featureParam/100+0.05, 7, cv::noArray(), 3, true, 0.04);
                for (unsigned int i=0; i<strong_corners.size(); i++){
                    cv::circle( proccessedImage, strong_corners[i], 3,  cv::Scalar(255,0,255), 2);
                }
            }else if (feature.compare("STAR")==0){
                cv::Mat gray;
                std::vector< cv::KeyPoint > keys;
                cv::StarDetector starDet(5, 10*this->featureParam/100, 5, 5, 10);
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                starDet.detect(gray, keys);
                cv::drawKeypoints(proccessedImage, keys, proccessedImage, cv::Scalar(0,255,255));
            }else if (feature.compare("FAST")==0){
                cv::Mat gray;
                std::vector< cv::KeyPoint > keys;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                cv::FAST(gray, keys, 100*this->featureParam/255, true);
                cv::drawKeypoints(proccessedImage, keys, proccessedImage, cv::Scalar(255,0,0));
            }else if (feature.compare("SIFT")==0){
                cv::Mat gray;
                std::vector< cv::KeyPoint > keys;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                cv::SIFT sifter(1, this->featureParam+1, 0.04, 10, 1.6);
                sifter.detect(gray, keys);
                cv::drawKeypoints(proccessedImage, keys, proccessedImage, cv::Scalar(0,0,255));
            }else if (feature.compare("SURF")==0){
                cv::Mat gray;
                std::vector< cv::KeyPoint > keys;
                cv::cvtColor(proccessedImage, gray, CV_BGR2GRAY);
                cv::SURF surfer(255*this->featureParam/100+1);
                surfer.detect(gray, keys);
                cv::drawKeypoints(proccessedImage, keys, proccessedImage, cv::Scalar(0,255,0));
            }
        }

        if (fundamentalMethod.compare("NONE",Qt::CaseSensitive)!=0){
            RobustMatcher rmatcher;
            rmatcher.setConfidenceLevel(0.98);
            rmatcher.setMinDistanceToEpipolar(1.0);
            rmatcher.setRatio(0.65f);
            cv::Ptr<cv::FeatureDetector> pfd=new cv::SurfFeatureDetector(10);
            rmatcher.setFeatureDetector(pfd);

            if (fundamentalMethod.compare("7POINT")==0){
                rmatcher.setMethod(CV_FM_7POINT);
                F = rmatcher.match(this->mview1,this->mview2,this->matches, this->keypoints1, this->keypoints2);
            }else if (fundamentalMethod.compare("8POINT")==0){
                rmatcher.setMethod(CV_FM_8POINT);
                F = rmatcher.match(this->mview1,this->mview2,this->matches, this->keypoints1, this->keypoints2);
            }else if (fundamentalMethod.compare("RANSAC")==0){
                rmatcher.setMethod(CV_FM_RANSAC);
                F = rmatcher.match(this->mview1,this->mview2,this->matches, this->keypoints1, this->keypoints2);
            }

            FM[0][0] = F.at<double>(0,0);
            FM[0][1] = F.at<double>(0,1);
            FM[0][2] = F.at<double>(0,2);
            FM[1][0] = F.at<double>(1,0);
            FM[1][1] = F.at<double>(1,1);
            FM[1][2] = F.at<double>(1,2);
            FM[2][0] = F.at<double>(2,0);
            FM[2][1] = F.at<double>(2,1);
            FM[2][2] = F.at<double>(2,2);
            fundamentalMethod="NONE";
        }

        if (stereo.compare("NONE",Qt::CaseSensitive)!=0){
            //Convert Keypoints
            std::vector<cv::Point2f> points1, points2;
            for (std::vector<cv::DMatch>::const_iterator it= matches.begin();it!= matches.end(); ++it) {
                // Get the position of left keypoints
                float x= keypoints1[it->queryIdx].pt.x;
                float y= keypoints1[it->queryIdx].pt.y;
                points1.push_back(cv::Point2f(x,y));
                // Get the position of right keypoints
                x= keypoints2[it->trainIdx].pt.x;
                y= keypoints2[it->trainIdx].pt.y;
                points2.push_back(cv::Point2f(x,y));
            }

            if (stereo.compare("EPIPOLAR")==0){
                std::vector<cv::Vec3f> lines;
                std::vector<cv::Point2f> points;

                if (this->showmview==1){
                    points = points1;
                    cv::computeCorrespondEpilines( cv::Mat(points),1,F, lines);
                    this->mview1.copyTo(proccessedImage);
                }else{
                    points = points2;
                    cv::computeCorrespondEpilines( cv::Mat(points),2,F, lines);
                    this->mview2.copyTo(proccessedImage);
                }

                for (std::vector<cv::Vec3f>::const_iterator it= lines.begin();
                it!=lines.end(); ++it) {
                    // draw the line between first and last column
                    cv::line(proccessedImage,
                    cv::Point(0,-(*it)[2]/(*it)[1]),
                    cv::Point(proccessedImage.cols,-((*it)[2]+
                    (*it)[0]*proccessedImage.cols)/(*it)[1]),
                    cv::Scalar(255,0,255));
                }

                for (unsigned int i=0; i<points.size();i++){
                    cv::circle(proccessedImage, points[i],3, cv::Scalar(255,255,0));
                }
            }else if (stereo.compare("HOMOGRAPHY")==0){
                std::vector<cv::Point2f> points1, points2;
                for (std::vector<cv::DMatch>::const_iterator it= matches.begin();it!= matches.end(); ++it) {
                    // Get the position of left keypoints
                    float x= keypoints1[it->queryIdx].pt.x;
                    float y= keypoints1[it->queryIdx].pt.y;
                    points1.push_back(cv::Point2f(x,y));
                    // Get the position of right keypoints
                    x= keypoints2[it->trainIdx].pt.x;
                    y= keypoints2[it->trainIdx].pt.y;
                    points2.push_back(cv::Point2f(x,y));
                }
                std::vector<uchar> inliers(points1.size(),0);
                H= cv::findHomography(cv::Mat(points1),cv::Mat(points2),inliers,CV_RANSAC,1.);

                std::vector<cv::Point2f>::const_iterator itPts;
                std::vector<uchar>::const_iterator itIn;

                if (this->showmview==1){
                    this->mview1.copyTo(proccessedImage);
                    itPts = points1.begin();
                    itIn= inliers.begin();
                    while (itPts!=points1.end()) {
                        // draw a circle at each inlier location
                        if (*itIn)
                        cv::circle(proccessedImage,*itPts,3,
                        cv::Scalar(255,255,255),2);
                        ++itPts;
                        ++itIn;
                    }
                }else{
                    this->mview2.copyTo(proccessedImage);
                    itPts= points2.begin();
                    itIn= inliers.begin();
                    while (itPts!=points2.end()) {
                    // draw a circle at each inlier location
                    if (*itIn)
                    cv::circle(proccessedImage,*itPts,3,
                    cv::Scalar(255,255,255),2);
                    ++itPts;
                    ++itIn;
                    }
                }
                HG[0][0] = H.at<double>(0,0);
                HG[0][1] = H.at<double>(0,1);
                HG[0][2] = H.at<double>(0,2);
                HG[1][0] = H.at<double>(1,0);
                HG[1][1] = H.at<double>(1,1);
                HG[1][2] = H.at<double>(1,2);
                HG[2][0] = H.at<double>(2,0);
                HG[2][1] = H.at<double>(2,1);
                HG[2][2] = H.at<double>(2,2);
            }else if (stereo.compare("MOSAIC")==0){
                // Warp image 1 to image 2
                cv::Mat result;
                cv::warpPerspective(this->mview1,result,H,cv::Size(2*this->mview1.cols,this->mview1.rows));
                // Copy image 1 on the first half of full image
                cv::Mat half(result,cv::Rect(0,0,this->mview2.cols,this->mview2.rows));
                this->mview2.copyTo(half); // copy image2 to image1 roi
                result.copyTo(proccessedImage);
            }else if (stereo.compare("MATCHES")==0){
                cv::drawMatches(this->mview1, this->keypoints1, this->mview2, this->keypoints2, this->matches,
                                proccessedImage);
            }
        }
        if (this->calibrate){
            if (this->calibImageIndex<this->numImagesCalibration){
                if (calibrator.addChessboardPoints(proccessedImage, cv::Size(9,6)) > 0)
                    this->calibImageIndex++;
            }else{
                if (!this->calibrated){
                    calibrator.calibrate(cv::Size(proccessedImage.rows, proccessedImage.cols));
                    E = calibrator.getCameraMatrix();
                    EC[0][0] = E.at<double>(0,0);
                    EC[0][1] = E.at<double>(0,1);
                    EC[0][2] = E.at<double>(0,2);
                    EC[1][0] = E.at<double>(1,0);
                    EC[1][1] = E.at<double>(1,1);
                    EC[1][2] = E.at<double>(1,2);
                    EC[2][0] = E.at<double>(2,0);
                    EC[2][1] = E.at<double>(2,1);
                    EC[2][2] = E.at<double>(2,2);
                    this->calibrated=true;
                }
                proccessedImage = calibrator.remap(proccessedImage);
                cv::putText(proccessedImage, "Undistortion Image", cv::Point(10,50), CV_FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(255,0,0),2);
            }
        }

        if (this->addImageToStitch){
            this->addImageToStitch=false;
            this->addImageToStitchFF=false;
            this->stitchImages.push_back(proccessedImage);
        }

        if (this->addImageToStitchFF){
            this->addImageToStitchFF=false;
            this->addImageToStitch=false;
            cv::Mat temp = cv::imread(this->stitchName.toStdString(), CV_LOAD_IMAGE_COLOR);
            this->stitchImages.push_back(temp);
        }

        if (this->stitch){
            cv::Stitcher stitcher = cv::Stitcher::createDefault();
            stitcher.stitch(this->stitchImages, proccessedImage);
        }

        if (this->addImageToSFM){
            this->addImageToSFM=false;
            this->addImageToSFMFF=false;
            this->sfmImages.push_back(proccessedImage);
            char name[200];
            sprintf(name,"image_%d",(int)this->imageIds.size()+1);
            this->imageIds.push_back(name);
        }

        if (this->addImageToSFMFF){
            this->addImageToSFMFF=false;
            this->addImageToSFM=false;
            cv::Mat temp = cv::imread(this->sfmName.toStdString(), CV_LOAD_IMAGE_COLOR);
            this->sfmImages.push_back(temp);
            char name[200];
            sprintf(name,"image_%d",(int)this->imageIds.size()+1);
            this->imageIds.push_back(name);
        }

        if (this->doSfm){
            cv::Ptr<MultiCameraPnP> sfm = new MultiCameraPnP(sfmImages,imageIds,"");
            sfm->use_gpu=false;
            sfm->use_rich_features = true;
            sfm->RecoverDepthFromImages();
            structure = sfm->getPointCloud();
            colors = sfm->getPointCloudRGB();
            if (structure.size()==0){
                structure=sfm->getPointCloudBeforeBA();
                colors=sfm->getPointCloudRGBBeforeBA();
            }

            for (unsigned int i=0; i<structure.size(); i++){
                std::cout<<"("<<structure.at(i).x<<","<<structure.at(i).y<<","<<structure.at(i).z<<")"<<std::endl;
            }
            this->doSfm = false;
            emit(sfmReady());
        }

        if (this->updateHistogram){
            cv::Mat histogram;
            histogram = drawHistogram(proccessedImage);
            qImage1Histogram = Mat2QImage(histogram);
        }

        qProccessedImage = Mat2QImage(proccessedImage);
    }
}

/* bool lock */
void ComputerVisionInterface::setLoopLock(bool state){
    this->loopLocked=state;
}

/** QT-OpenCV interface **/
QImage ComputerVisionInterface::Mat2QImage(IplImage *input){
    QImage image(input->width, input->height, QImage::Format_RGB32);
    uchar* pBits = image.bits();
    int nBytesPerLine = image.bytesPerLine();
    for (int n = 0; n < input->height; n++){
        for (int m = 0; m < input->width; m++){
            CvScalar s = cvGet2D(input, n, m);
            QRgb value = qRgb((uchar)s.val[2], (uchar)s.val[1], (uchar)s.val[0]);

            uchar* scanLine = pBits + n * nBytesPerLine;
            ((uint*)scanLine)[m] = value;
        }
    }
    return image;
}

QImage ComputerVisionInterface::Mat2QImage(cv::Mat &mat){
    IplImage input(mat);
    QImage image(input.width, input.height, QImage::Format_RGB32);
    uchar* pBits = image.bits();
    int nBytesPerLine = image.bytesPerLine();
    for (int n = 0; n < input.height; n++){
        for (int m = 0; m < input.width; m++){
            CvScalar s = cvGet2D(&input, n, m);
            QRgb value = qRgb((uchar)s.val[2], (uchar)s.val[1], (uchar)s.val[0]);

            uchar* scanLine = pBits + n * nBytesPerLine;
            ((uint*)scanLine)[m] = value;
        }
    }
    return image;
}

/** Setters **/
void ComputerVisionInterface::setIm2Show(int i){
    this->showmview = i;
}

void ComputerVisionInterface::setFeatureParam(double v){
    this->featureParam=v;
}

void ComputerVisionInterface::setThreshold(double v){
    this->threshold=v;
}

void ComputerVisionInterface::setHoughParams(double h){
    this->setLoopLock(true);
    this->houghParam=h;
}

void ComputerVisionInterface::setCannyParams(double c1, double c2){
    this->cannyParam1=c1;
    this->cannyParam2=c2;
}

void ComputerVisionInterface::setFilterParam(double val/*min=0, max=99*/){
    this->setLoopLock(true);
    this->filterParam = val;
}

void ComputerVisionInterface::setNoisePower(int val/*min=0, max=99*/){
    this->setLoopLock(true);
    this->noisePower = val;
}

void ComputerVisionInterface::setNoiseStdDev(int val/*min=0, max=99*/){
     this->setLoopLock(true);
    this->noiseStdDev = val;
}

bool ComputerVisionInterface::setVideoCapturer1(int device){
    capture1.open(device);
    return capture1.isOpened();
}

bool ComputerVisionInterface::setVideoCapturer2(int device){
    capture2.open(device);
    return capture2.isOpened();
}

void ComputerVisionInterface::freeVideoCapturer1(){
    if (&capture1!=NULL){
        capture1.release();
    }
}

void ComputerVisionInterface::freeVideoCapturer2(){
    if (&capture2!=NULL){
        capture2.release();
    }
}

/** Available Proccesses **/
void ComputerVisionInterface::calibrateCam(bool f){
    this->calibrate=f;
}

void ComputerVisionInterface::selectView1(bool v){
    this->bmview1 = v;
}

void ComputerVisionInterface::selectView2(bool v){
    this->bmview2 = v;
}

void ComputerVisionInterface::computeFundamentalMatrix(QString m){
    this->fundamentalMethod = m;
}

void ComputerVisionInterface::findFeature(QString type){
    this->feature=type;
}

void ComputerVisionInterface::findShapeDescriptor(QString type){
    this->shape=type;
}

void ComputerVisionInterface::findContours(bool v){
    this->contours=v;
}

void ComputerVisionInterface::findConObjs(bool v){
    this->conObjs=v;
}

void ComputerVisionInterface::setSaltPepperNoise(bool activated, int power/*min=0, max=99*/){
    this->setLoopLock(true);
    this->addSaltPepperNoise = activated;
    this->noisePower = power;
}

void ComputerVisionInterface::setGaussianNoise(bool activated, int power/*min=0, max=99*/, int stddev/*min=0, max=99*/){
    this->setLoopLock(true);
    this->addGaussianNoise = activated;
    this->noisePower = power;
    this->noiseStdDev = stddev;
}

void ComputerVisionInterface::rgbToGray(bool active){
    this->setLoopLock(true);
    this->convertToGray=active;
}

void ComputerVisionInterface::setUpdateHistogram(bool active){
    this->setLoopLock(true);
    this->updateHistogram=active;
}

void ComputerVisionInterface::setHistogramEqualization(bool active){
    this->setLoopLock(true);
    this->equalizeHistogram = active;
}

void ComputerVisionInterface::setRGBToHLS(bool act){
    this->setLoopLock(true);
    this->rgbToHls=act;
}

void ComputerVisionInterface::setRGBToXYZ(bool act){
    this->setLoopLock(true);
    this->rgbToXyz=act;
}

void ComputerVisionInterface::setRGBToYCbCr(bool act){
    this->setLoopLock(true);
    this->rgbToYcbcr=act;
}

void ComputerVisionInterface::setRGBToHSV(bool act){
    this->setLoopLock(true);
    this->rgbToHsv=act;
}

void ComputerVisionInterface::setRGBToLAB(bool act){
    this->setLoopLock(true);
    this->rgbToLab=act;
}

void ComputerVisionInterface::setRGBToLUV(bool act){
    this->setLoopLock(true);
    this->rgbToLuv=act;
}

void ComputerVisionInterface::setLogo(bool act, QString filename, double x, double y){
    this->setLoopLock(true);
    this->xlogo=x;
    this->ylogo=y;
    this->logoActivated = act;
    this->logoFilename = filename.toStdString();
}

void ComputerVisionInterface::setLogoPosition(double x, double y){
    this->setLoopLock(true);
    this->xlogo=x;
    this->ylogo=y;
}

void ComputerVisionInterface::setLogoTransparency(int v){
    this->transparency = v;
}

void ComputerVisionInterface::applyStereoFun(QString type){
    this->stereo = type;
}

void ComputerVisionInterface::applyMorpho(QString type, double param){
    this->morpho = type;
    this->morphoSize=param;
}

void ComputerVisionInterface::setMorphoSize(double param){
    this->morphoSize=param;
}

void ComputerVisionInterface::applyFilter(QString type, double param){
    this->filter = type;
    this->filterParam = param;
}

void ComputerVisionInterface::applyHough(QString type, double param){
    this->hough = type;
    this->houghParam=param;
}

void ComputerVisionInterface::applyCanny(bool canny, double c1, double c2){
    this->setLoopLock(true);
    this->canny = canny;
    this->cannyParam1=c1;
    this->cannyParam2=c2;
}

/** Auxiliar Functions **/
void ComputerVisionInterface::startSFM(bool v){
    this->doSfm=v;
}

void ComputerVisionInterface::clearSFM(){
    this->sfmImages.clear();
    this->imageIds.clear();
}

void ComputerVisionInterface::addFrameToSFMFromFile(QString name){
    this->addImageToSFMFF=true;
    this->sfmName=name;
}

void ComputerVisionInterface::addFrameToSFM(){
    this->addImageToSFM=true;
}

void ComputerVisionInterface::startStitcher(bool v){
    this->stitch=v;
}

void ComputerVisionInterface::clearStitcher(){
    this->stitchImages.clear();
}

void ComputerVisionInterface::addFrameToStitcherFromFile(QString name){
    this->addImageToStitchFF=true;
    this->stitchName=name;
}

void ComputerVisionInterface::addFrameToStitcher(){
    this->addImageToStitch=true;
}

void ComputerVisionInterface::setWorkingOnCam(bool act){
    this->workingOnCam=act;
}

void ComputerVisionInterface::setWorkingOnFrame(bool act){
    this->workingOnFrame=act;
}

void ComputerVisionInterface::setWorkingOnVideo(bool act){
    this->workingOnVideo=act;
}

bool ComputerVisionInterface::getEndVideo(){
    return this->endVideo;
}

cv::Mat ComputerVisionInterface::drawHistogram(cv::Mat src){
    /// Separate the image in 3 places ( B, G and R )
    std::vector<cv::Mat> bgr_planes;
    split( src, bgr_planes );
    /// Establish the number of bins
    int histSize = 256;
    bool uniform = true; bool accumulate = false;


    /// Set the ranges ( for B,G,R) )
    float range[] = { -5, 260 } ;
    const float* histRange = { range };

    cv::Mat b_hist, g_hist, r_hist;

    /// Compute the histograms:
    calcHist( &bgr_planes[0], 1, 0, cv::Mat(), b_hist, 1, &histSize, &histRange, uniform, accumulate );
    calcHist( &bgr_planes[1], 1, 0, cv::Mat(), g_hist, 1, &histSize, &histRange, uniform, accumulate );
    calcHist( &bgr_planes[2], 1, 0, cv::Mat(), r_hist, 1, &histSize, &histRange, uniform, accumulate );

    /// Draw the histograms for B, G and R
    int hist_w = 512; int hist_h = 400;
    int bin_w = cvRound( (double) hist_w/histSize );

    cv::Mat histImage( hist_h, hist_w, CV_8UC3, cv::Scalar( 128,128,128 ) );

    /// Normalize the result to [ 0, histImage.rows ]
    cv::normalize(b_hist, b_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );
    cv::normalize(g_hist, g_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );
    cv::normalize(r_hist, r_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );

    /// Draw for each channel
    for( int i = 1; i < histSize; i++ ){
       line( histImage, cv::Point( bin_w*(i-1), hist_h - cvRound(b_hist.at<float>(i-1)) ) ,
                        cv::Point( bin_w*(i), hist_h - cvRound(b_hist.at<float>(i)) ),
                        cv::Scalar( 255, 0, 0), 4, 8, 0  );
       line( histImage, cv::Point( bin_w*(i-1), hist_h - cvRound(g_hist.at<float>(i-1)) ) ,
                        cv::Point( bin_w*(i), hist_h - cvRound(g_hist.at<float>(i)) ),
                        cv::Scalar( 0, 255, 0), 4, 8, 0  );
       line( histImage, cv::Point( bin_w*(i-1), hist_h - cvRound(r_hist.at<float>(i-1)) ) ,
                        cv::Point( bin_w*(i), hist_h - cvRound(r_hist.at<float>(i)) ),
                        cv::Scalar( 0, 0, 255), 4, 8, 0  );
    }
    return histImage;
}

void ComputerVisionInterface::setFrameFilename(QString filename){
    this->frameFilename = filename.toStdString();
}

void ComputerVisionInterface::setVideoFilename(QString filename){
    this->videoFilename = filename.toStdString();
}

void ComputerVisionInterface::stopThis(){
    this->loopLocked=false;
}

/** END **/
