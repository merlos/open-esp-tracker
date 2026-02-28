require "test_helper"

class Api::V1::LocationsControllerTest < ActionDispatch::IntegrationTest
  def setup
    @device = Device.create!(name: "api-test-device", active: true)
    @token = @device.regenerate_api_token
  end

  test "creates location with valid token" do
    post "/api/v1/locations",
      params: { latitude: 51.5074, longitude: -0.1278, recorded_at: Time.current },
      headers: { "Authorization" => "Bearer #{@token}", "Content-Type" => "application/json" },
      as: :json

    assert_response :created
    assert_equal 1, @device.locations.count
  end

  test "rejects request without token" do
    post "/api/v1/locations",
      params: { latitude: 51.5074, longitude: -0.1278 },
      as: :json

    assert_response :unauthorized
  end

  test "rejects request with invalid token" do
    post "/api/v1/locations",
      params: { latitude: 51.5074, longitude: -0.1278 },
      headers: { "Authorization" => "Bearer invalidtoken123456789" },
      as: :json

    assert_response :unauthorized
  end
end
