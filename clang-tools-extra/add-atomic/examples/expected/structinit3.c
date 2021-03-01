struct S {
  int _Atomic * a;
  int _Atomic * b;
};

struct T {
  struct S s1;
  struct S s2;
};

void foo() {
  int _Atomic  target;
  int _Atomic  c;
  struct T myT = {{&target, &c}, {&target, &target}};
  (void)myT;
}
