struct S {
  int target;
};

void foo() {
  struct S myS;
  int* foo = &myS.target;
  (void)foo;
}
