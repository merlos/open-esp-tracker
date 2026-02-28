admin = User.find_or_create_by!(email: 'admin@example.com') do |u|
  u.name = 'Admin User'
  u.password = 'changeme123'
  u.password_confirmation = 'changeme123'
  u.role = :admin
end

user = User.find_or_create_by!(email: 'user@example.com') do |u|
  u.name = 'Regular User'
  u.password = 'changeme123'
  u.password_confirmation = 'changeme123'
  u.role = :regular
end

device = Device.find_or_create_by!(name: 'ESP-Tracker-001') do |d|
  d.user = admin
  d.active = true
end

puts "Seed complete: #{User.count} users, #{Device.count} devices"
