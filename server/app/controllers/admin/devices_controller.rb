module Admin
  class DevicesController < ApplicationController
    before_action :require_login
    before_action :require_admin

    def index
      @devices = Device.all.order(:name)
    end

    def new
      @device = Device.new
    end

    def create
      @device = Device.new(device_params)
      if @device.save
        plain_token = @device.regenerate_api_token
        redirect_to admin_devices_path, notice: "Device created. API Token: #{plain_token} (save it now!)"
      else
        render :new, status: :unprocessable_entity
      end
    end

    def destroy
      @device = Device.find(params[:id])
      @device.destroy
      redirect_to admin_devices_path, notice: "Device deleted."
    end

    private

    def device_params
      params.require(:device).permit(:name, :user_id, :active)
    end
  end
end
