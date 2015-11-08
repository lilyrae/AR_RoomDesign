#ifndef PARSER_H_
#define PARSER_H_

#include <vector>
#include <fstream>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>

using namespace std;

void readTeamsFromFile(string filePath, map<string, int> &ARMarkersList,
		map<string, bool> &ARMarkersState, map<string, float> &ARMarkersScale,
		map<string, float> &ARMarkersRotation, map<string, string> &ARMarkersMesh);
vector<string> split(const string &s, char delim);

#endif /* PARSER_H_ */
