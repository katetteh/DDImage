#include <iostream>
#include <vector>

using namespace std;

class Solution {
public:
    vector<int> spiralOrder(vector<vector<int>>& matrix) {
        int rows = matrix.size();
        int cols = matrix[0].size();

        int leftWall = 0;
        int rightWall = cols;
        int downWall = rows;
        int upWall = 0;
        vector<int> result;
        while (result.size() < rows*cols){
                for (int i = leftWall; i < rightWall; i++){
                    result.push_back(matrix[upWall][i]);
                }
                upWall++;
                rightWall -= 1;
                while(j < downWall && j > upWall){
                    result.push_back(matrix[j][i]);
                    j++;
                }
                downWall -= 1;
                i--;
                while(i > leftWall && i < rightWall){
                    result.push_back(matrix[j][i]);
                    i--;
                }
                leftWall += 1;

                j--;
                while(j > upWall && j < downWall){
                    result.push_back(matrix[j][i]);
                    j--;
                }
                upWall += 1;
            }    
                
        }
        return result;
    }
};