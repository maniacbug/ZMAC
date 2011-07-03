BEGIN {
  print "#include <DEFINES>"
}
{
  gsub(/== HIGH/,"== pin_HIGH")
  gsub(/= LOW/,"= pin_LOW")
  gsub(/= HIGH/,"= pin_HIGH")
  $0 = gensub(/(for ?\()(\w+ )(\w+)/,"\\2\\3 ; \\1\\3","g",$0)
  print $0
}
