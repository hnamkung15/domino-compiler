typedef struct Packet {
  int x;
  int y;
  int z;
  int w;
} Packet;

void func(Packet p) {
  p.x = 1;
  p.y = 2;
  if (p.x == 3) {
    if (p.y == 5) {
      p.z = 3;
    } else  {
      p.w = 2;
    }
  } else {
    ;
  }
}