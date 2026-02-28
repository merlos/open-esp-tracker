class Location < ApplicationRecord
  belongs_to :device

  validates :device_id, presence: true
  validates :latitude, presence: true
  validates :longitude, presence: true
end
