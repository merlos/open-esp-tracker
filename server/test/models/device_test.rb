require "test_helper"

class DeviceTest < ActiveSupport::TestCase
  def setup
    @device = Device.new(name: "Test-Device-001")
  end

  test "valid device" do
    assert @device.valid?
  end

  test "name required" do
    @device.name = ""
    assert_not @device.valid?
  end

  test "name uniqueness" do
    @device.save!
    duplicate = Device.new(name: "Test-Device-001")
    assert_not duplicate.valid?
  end

  test "regenerate_api_token returns plain token" do
    @device.save!
    token = @device.regenerate_api_token
    assert token.present?
    assert_equal 64, token.length
  end

  test "verify_api_token returns true for correct token" do
    @device.save!
    token = @device.regenerate_api_token
    assert @device.verify_api_token(token)
  end

  test "verify_api_token returns false for wrong token" do
    @device.save!
    @device.regenerate_api_token
    assert_not @device.verify_api_token("wrongtoken")
  end

  test "api_token_prefix set on regenerate" do
    @device.save!
    token = @device.regenerate_api_token
    assert_equal token[0, 8], @device.api_token_prefix
  end
end
