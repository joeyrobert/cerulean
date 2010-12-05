programs = ['cerulean-gcc.exe', 'cerulean-vs.exe', 'cerulean-intel.exe']
run_times = {}
multiple = ARGV[0].to_i

multiple.times do
  programs.each do |program|
    puts "#{Time.now}: Starting #{program}"
    start_time = Time.now.to_f
    `#{program}`
    end_time = Time.now.to_f
    run_times[program] ||= []
    run_times[program] << (end_time - start_time)
  end
end

p run_times

programs.each do |program|
  puts "[#{program}] Average running time: #{run_times[program].inject(:+) / multiple.to_f} s"
end
