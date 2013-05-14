#!/usr/bin/awk -f
{
  if( cf == 0 ) {
    cf = $2;
  }
  score[NR] = $2 - cf;
  line[NR] = $0;
}

END{
  # randomizer seeded by time of day
  # don't use more often than once per second.
  srand();
  while(1) {
    x = int(rand() * NR) + 1;
    t = -log(rand());
    if( log(2) * score[x] < t ) {
      print line[x];
      break;
    }
  }
}
