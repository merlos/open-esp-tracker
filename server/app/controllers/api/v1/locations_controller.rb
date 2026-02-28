module Api
  module V1
    class LocationsController < ActionController::API
      before_action :authenticate_device

      def create
        location = @device.locations.build(location_params)
        if location.save
          render json: { status: "ok", id: location.id }, status: :created
        else
          render json: { error: location.errors.full_messages }, status: :unprocessable_entity
        end
      end

      private

      def authenticate_device
        auth_header = request.headers["Authorization"]
        token = auth_header&.split(" ")&.last
        unless token
          render json: { error: "Missing token" }, status: :unauthorized
          return
        end
        prefix = token[0, 8]
        @device = Device.find_by(api_token_prefix: prefix, active: true)
        unless @device&.verify_api_token(token)
          render json: { error: "Invalid token" }, status: :unauthorized
        end
      end

      def location_params
        params.permit(:latitude, :longitude, :altitude, :speed, :accuracy,
                      :battery_level, :battery_voltage, :satellites, :hdop,
                      :firmware_version, :recorded_at, :battery_low)
      end
    end
  end
end
