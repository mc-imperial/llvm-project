struct S {
  int _Atomic * a;
  int* b;
};

struct T {
  struct S s1;
  struct S s2;
};

void foo() {
  int _Atomic  target;
  int c;
  struct T myT = {{&target, &c}, {&target, &c}};
  (void)myT;
}
