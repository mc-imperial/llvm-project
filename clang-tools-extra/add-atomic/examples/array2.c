void foo() {
  int target;
  int y;
  int z;
  int* A[3][3] = {{&target, &target, &target}, {&y, &y, &y}, {&z, &z, &z}};
  int* p = 0;
  A[0][0] = p;
  int** q = &A[2][1];
  (void)q;
}
