class DevicesController < ApplicationController
  before_action :require_login
  before_action :set_device, only: [:show, :destroy, :generate_token]

  def index
    @devices = Device.includes(:locations).order(:name)
    @devices_with_location = @devices.map do |device|
      last_location = device.locations.order(recorded_at: :desc).first
      { device: device, last_location: last_location }
    end
  end

  def show
    @last_location = @device.locations.order(recorded_at: :desc).first
    @recent_locations = @device.locations.order(recorded_at: :desc).limit(50)
  end

  def new
    require_admin
    @device = Device.new
  end

  def create
    require_admin
    @device = Device.new(device_params)
    if @device.save
      redirect_to devices_path, notice: "Device created."
    else
      render :new, status: :unprocessable_entity
    end
  end

  def destroy
    require_admin
    @device.destroy
    redirect_to devices_path, notice: "Device deleted."
  end

  def generate_token
    require_admin
    plain_token = @device.regenerate_api_token
    redirect_to device_path(@device), notice: "New token generated: #{plain_token} (save it now, it won't be shown again)"
  end

  private

  def set_device
    @device = Device.find(params[:id])
  end

  def device_params
    params.require(:device).permit(:name, :user_id, :active)
  end
end
