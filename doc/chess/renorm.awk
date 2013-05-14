#!/usr/bin/awk -f
{
  if( cf == 0 ) {
    cf = $2
  }
  print $2 - cf, $0
}
