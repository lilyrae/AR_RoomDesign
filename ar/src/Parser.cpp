#include "Parser.h"

vector<string> &split(const string &s, char delim, vector<string> &elems) {
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		if (!item.empty())
			elems.push_back(item);
	}
	return elems;
}

vector<string> split(const string &s, char delim) {
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}

void readTeamsFromFile(string filePath, map<string, int> &ARMarkersList,
		map<string, bool> &ARMarkersState, map<string, float> &ARMarkersScale,
		map<string, float> &ARMarkersRotation,
		map<string, string> &ARMarkersMesh) {

	ifstream logReader(filePath.c_str(), ofstream::in);

	string line;

	int i = 0;

	while (getline(logReader, line)) {
		// end of the file
		if (line.find("##") != string::npos)
			break;
		// comment
		else if (line.find("#") != string::npos)
			;
		else {
			vector<string> data = split(line, ';');

			string id = data[0];

			// add index
			ARMarkersList[id] = i;

			// add scale
			ARMarkersScale[id] = atof(data[1].c_str());

			// add rotation
			ARMarkersRotation[id] = atof(data[2].c_str());

			// add mesh name
			ARMarkersMesh[id] = data[3];

			cout << "Adding id: " << id << " - mesh: " << data[3]
					<< " at index: " << i << " - Scale: " << data[1].c_str() << " - Rot: " << data[2].c_str() << endl;

			i++;
		}

	}

}

