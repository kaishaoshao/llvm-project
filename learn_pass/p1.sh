echo "int main(){
	int a = 5;
	int b = a * 2;
	int c = a + b * 4;

	return 0;
}" > /tmp/test_p1.cpp

opt=" -emit-llvm -S"

../build/bin/clang++ $opt /tmp/test_p1.cpp   

