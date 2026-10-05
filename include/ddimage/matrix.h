#ifndef matrix_h
#define matrix_h
#include <iostream>
#include <vector>
#include <string>

using namespace std;
class Matrix{
    public:
        Matrix(int rows, int cols):arr(rows){
            for(auto& thisRow: arr){
                thisRow.resize(cols);
            }
        }
        vector<int>& operator[](int index) {
            return arr[index];
        }
        int numRows() const{
            return arr.size();
        }
        int numCols() const{
            return numRows() ? arr[0].size(): 0;
        }
        void display() const{
            for(int i = 0; i < arr.size(); ++i){
                for(int j = 0; j < arr[0].size(); ++j){
                    cout<< arr[i][j]<< "\t";
                }
                cout<<"\n";
            }
            cout<<"\n";
        }
        bool isEmpty(){
            return ((numRows() == 0 || numCols() == 0) ? true: false);
        }
        void insert(int x, int row, int col){
            arr[row][col] = x;
        }

        Matrix transpose() {
            int rows = numRows();
            int cols = numCols();
            Matrix answer(cols, rows);
            for(
                int i = 0; i < rows; i++){
                for(int j = 0; j < cols; j++){
                    answer[j][i] = arr[i][j];
                }
            }
                return answer;
        };
        Matrix operator+(Matrix & other){
            if(numRows()== other.numRows() && numCols()== other.numCols()){
                Matrix answer(numRows(), numCols());
                for(int i = 0; i < numRows(); i++){
                    for(int j = 0; j < numCols(); j++){    
                        answer[i][j] = (*this)[i][j] + other[i][j];
                    }
                }
                return answer;
            }else{
                throw runtime_error("Matrix dimension doesn't match");
            }
        }
        Matrix operator-(Matrix & other){
            if(numRows()== other.numRows() && numCols()== other.numCols()){
                Matrix answer(numRows(), numCols());
                for(int i = 0; i < numRows(); i++){
                    for(int j = 0; j < numCols(); j++){    
                        answer[i][j] = (*this)[i][j] - other[i][j]; 
                    }
                }
                return answer;
            }else{
                throw runtime_error("Matrix dimension doesn't match");
            }
        }
        vector<int> find_sorted(int target){
            if(!isEmpty()){
                int m = numRows();
                int n = numCols();
                int low = 0;
                int high = (m*n)-1;
                vector<int> indexes;

                while(low <= high){
                    int mid = low + (high - low)/2;
                    int row = mid/n;
                    int col = mid%n;
                    int midValue = arr[row][col];
                    if (target == midValue){
                         indexes = {row, col};
                         break;
                    }else if(midValue < target){
                        low = mid + 1;
                    }else{
                        high = mid - 1;
                    }
                }
                return indexes;
            }
            else{
                throw logic_error("Matrix is empty hence can't be searched");
            }
        }
        vector<int> find(int target){
            if(!isEmpty()){
                vector<int> answer;
                int rows = numRows();
                int cols = numCols();
                for(int i = 0; i < rows; i++){
                    for(int j = 0; j < cols; j++){
                        if (target == arr[i][j]){
                            answer = {i, j};
                            return answer;
                        }
                    }
                }
                return answer;
            }else{
                throw logic_error("Matrix is empty hence can't be searched");
            }
        }
        
    private:
        vector<vector <int> > arr;
};

#endif
