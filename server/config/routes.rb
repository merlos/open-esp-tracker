Rails.application.routes.draw do
  root 'devices#index'
  resource :session, only: [:new, :create, :destroy]
  resources :devices do
    member do
      post :generate_token
    end
    resources :locations, only: [:index]
  end
  namespace :admin do
    resources :users
    resources :devices, only: [:index, :new, :create, :destroy]
  end
  namespace :api do
    namespace :v1 do
      resources :locations, only: [:create]
    end
  end
end
