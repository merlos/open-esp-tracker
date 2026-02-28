require "test_helper"

class LocationTest < ActiveSupport::TestCase
  def setup
    @device = Device.create!(name: "loc-test-device")
    @location = Location.new(device: @device, latitude: 51.5074, longitude: -0.1278)
  end

  test "valid location" do
    assert @location.valid?
  end

  test "device required" do
    @location.device = nil
    assert_not @location.valid?
  end

  test "latitude required" do
    @location.latitude = nil
    assert_not @location.valid?
  end

  test "longitude required" do
    @location.longitude = nil
    assert_not @location.valid?
  end
end
