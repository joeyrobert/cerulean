BITS = {
  'CAPTURE'     => 0x01000000,
  'CASTLE'      => 0x02000000,
  'EN_PASSANT'  => 0x04000000,
  'PAWN_DOUBLE' => 0x08000000,
  'PAWN_MOVE'   => 0x10000000,
  'PROMOTE'     => 0x20000000
}

MOVE_TO            = 0x000000FF
MOVE_FROM          = 0x0000FF00
MOVE_PROMOTE       = 0x00FF0000
MOVE_BITS          = 0xFF000000

move = ARGV[0].to_i

puts "TO:      #{(move & MOVE_TO)}"
puts "FROM:    #{((move & MOVE_FROM) >> 8)}"
puts "PROMOTE: #{((move & MOVE_PROMOTE) >> 16)}"
print "BITS:    #{((move & MOVE_BITS)>>24).to_s(2)} ("
print BITS.select { |k,v| (move & v) != 0 }.map { |(k, v)| k }.join(', ')
puts ")\n\n"
