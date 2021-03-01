struct S {
  int* f;
};

void foo() {
  int target;
  struct S myS = {&target};
  (void)myS;
}
