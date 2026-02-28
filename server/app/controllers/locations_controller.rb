class LocationsController < ApplicationController
  before_action :require_login

  def index
    @device = Device.find(params[:device_id])
    @locations = @device.locations.order(recorded_at: :desc).limit(100)
  end
end
