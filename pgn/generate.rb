require 'yajl'
require 'yajl/json_gem'

RESULTS = {"1-0"     => :white,
           "1/2-1/2" => :draw,
           "0-1"     => :black}
WINGEX = /(1-0|0-1|1\/2-1\/2)/
OPENING_LENGTH = 16
OPENING_THRESHOLD = 50

class PGNHotness
  attr_reader :games, :openings

  def initialize(opts = {})
    @openings = {}
    @count = 0

    if opts.key?(:pgn)
      @pgn = opts[:pgn] 
      @file = File.open(@pgn, 'r')
    elsif opts.key?(:json)
      @openings = Yajl::Parser.new(:symbolize_keys => true).parse(File.new(opts[:json], 'r'))
      @count = @openings.inject(0) {|sum, item| sum + item[1][:count] }
    end

    @game_info = {}
    @game = nil
  end

  def run
    while line = @file.gets
      begin
        if line =~ /\[(.*?) \"(.*?)\"\]/
          k = $1; v = $2
          @game_info[k] = (v =~ /^\d+$/) ? v.to_i : v
          next
        end
      rescue Exception => e
        puts "#{Time.now}: Illegal sequence found. Skipping. #{line.inspect}"
        next
      end

      @game += line if @game
      @game = line if line[0, 2] == '1.'

      if line =~ WINGEX && @game
        @game.gsub!(WINGEX, '')
        @game.gsub!(/(\{(.*?)\}|\r\n|\d+\.)/m, '')

        opening_moves = @game.split[0,OPENING_LENGTH].join(' ')
        opening = @openings[opening_moves]#.find { |i| i[:moves] == opening_moves }

        if opening.nil?
          hash = {
            :count => 1,
            :average_white => @game_info['WhiteElo'],
            :average_black => @game_info['BlackElo'],
            :average_elo => (@game_info['BlackElo']+@game_info['WhiteElo'])/2,
            :white => 0, :black => 0, :draw => 0
          }
          hash[RESULTS[@game_info['Result']]] += 1
          @openings[opening_moves] = hash
        else
          opening[:average_white] = (opening[:average_white]*opening[:count] + @game_info['WhiteElo'])/(opening[:count] + 1) 
          opening[:average_black] = (opening[:average_black]*opening[:count] + @game_info['BlackElo'])/(opening[:count] + 1) 
          opening[:average_elo] = (opening[:average_elo]*opening[:count] + (@game_info['WhiteElo']+@game_info['BlackElo'])/2)/(opening[:count] + 1) 
          opening[:count] += 1
          opening[RESULTS[@game_info['Result']]] += 1
        end

        @game = nil
        @count += 1
        puts "#{Time.now}: Parsed game #{@count}, Position #{@file.pos} of #{@file.stat.size} (#{'%2.2f' % (@file.pos / @file.stat.size.to_f * 100)}%)" if @count % 1000 == 0
      end
    end

    File.open(@pgn.gsub(/\.pgn/, '.json'), 'w') { |f| f.write(@openings.to_json) }
  end

  def display
    openings = @openings.select { |k, v| v[:count] >= OPENING_THRESHOLD }
    puts "OPENING ANALYSIS (#{@count} games, #{OPENING_LENGTH} deep, #{OPENING_THRESHOLD} games minimum)"
    puts "\nHIGHEST AVERAGE ELO"
    display_openings openings.sort {|a, b| b[1][:average_elo] <=> a[1][:average_elo] }
    puts "\nMOST COMMON OPENINGS"
    display_openings openings.sort {|a, b| b[1][:count] <=> a[1][:count] }
    puts "\nBEST FOR WHITE:"
    display_openings openings.sort {|a, b| (b[1][:white]/b[1][:count].to_f) <=> (a[1][:white]/a[1][:count].to_f) }
    puts "\nBEST FOR BLACK:"
    display_openings openings.sort {|a, b| (b[1][:black]/b[1][:count].to_f) <=> (a[1][:black]/a[1][:count].to_f) }
  end

  def display_openings(openings)
    puts "Elo    #      W (%)   B (%)   D (%)   Moves"
    openings[0,20].each do |(moves, opening)|
      print "#{'%4d' % opening[:average_elo]}   "
      print "#{'%4d' % opening[:count]}   "
      print "#{'%5.2f' % (opening[:white]/opening[:count].to_f*100)}   "
      print "#{'%5.2f' % (opening[:black]/opening[:count].to_f*100)}   "
      print "#{'%5.2f' % (opening[:draw]/opening[:count].to_f*100)}   "
      puts "#{moves}"
    end
  end
end

if ARGV.length == 1
  if ARGV[0] =~ /\.pgn$/
    pgn = PGNHotness.new(:pgn => ARGV[0])
    pgn.run
    pgn.display
  elsif ARGV[0] =~ /\.json$/
    pgn = PGNHotness.new(:json => ARGV[0])
    pgn.display
  end
else
  puts "Usage: ruby #{__FILE__} FILENAME"
end
