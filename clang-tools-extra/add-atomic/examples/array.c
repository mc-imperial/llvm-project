void foo() {
  int target;
  int y;
  int z;
  int* A[3] = {&target, &y, &z};
  int* p = 0;
  A[0] = p;
  int** q = &A[2];
  (void)q;
}
