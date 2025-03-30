import React, { useEffect, useState } from "react";
import { Box, Flex, useColorModeValue, useToast } from "@chakra-ui/react";
import Navbar from "../components/Navbar";
import Sidepanel from "../components/Sidepanel";
import Map from "../components/Map";

const Dashboard = () => {
  const [vehicles, setVehicles] = useState([]);
  const [selectedVehicle, setSelectedVehicle] = useState(null);
  const [displayedRoutes, setDisplayedRoutes] = useState([]);
  const toast = useToast();

  const toggleRouteVisibility = async (routeId, isVisible, vehicle) => {
    const existingRoute = displayedRoutes.find((r) => r.routeId === routeId);
    if (existingRoute) {
      setDisplayedRoutes((prev) =>
        prev.map((r) =>
          r.routeId === routeId ? { ...r, hidden: !isVisible } : r
        )
      );
      return;
    }
    if (!isVisible) return;

    const token = localStorage.getItem("authToken");
    try {
      const response = await fetch(
        `https://webapi.vehiclemap.xyz/routes/${routeId}/positions`,
        {
          headers: { Authorization: `Bearer ${token}` },
        }
      );
      if (!response.ok) {
        throw new Error(`Failed to fetch route positions`);
      }
      const positions = await response.json();
      setDisplayedRoutes((prev) => [
        ...prev,
        {
          routeId,
          vehicleColor: vehicle.color,
          vehicleId: vehicle.id,
          positions,
          hidden: false,
        },
      ]);
    } catch (error) {
      console.error("Error fetching positions:", error);
      toast({
        title: "Error",
        description: "Could not load positions for route: " + routeId + ".",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
    }
  };

  const clearRoute = (routeId) => {
    setDisplayedRoutes((prev) => prev.filter((r) => r.routeId !== routeId));
  };

  const bg = useColorModeValue("gray.50", "gray.900");

  const refreshVehicles = () => {
    const token = localStorage.getItem("authToken");
    if (!token) return;
    fetch("https://webapi.vehiclemap.xyz/vehicles/last-position", {
      headers: { Authorization: `Bearer ${token}` },
    })
      .then((response) => {
        if (!response.ok)
          throw new Error(`Error fetching vehicles: ${response.status}`);
        return response.json();
      })
      .then((data) => {
        const transformed = data.map((item) => ({
          ...item.vehicle,
          last_position: item.last_position,
        }));
        setVehicles(transformed);
      })
      .catch((err) => console.error(err));
  };

  useEffect(() => {
    refreshVehicles();
  }, []);

  const handleDelete = (id) => {
    setVehicles((prev) => prev.filter((v) => v.id !== id));
  };

  const handleUpdate = (updatedVehicle) => {
    setVehicles((prev) =>
      prev.map((v) => (v.id === updatedVehicle.id ? updatedVehicle : v))
    );
  };

  const handleShowOnMap = (vehicle) => {
    setSelectedVehicle(vehicle);
  };

  return (
    <Box minH="100vh" bg={bg}>
      <Navbar username={localStorage.getItem("username") || "User"} />
      <Flex>
        <Sidepanel
          vehicles={vehicles}
          refreshVehicles={refreshVehicles}
          onDelete={handleDelete}
          onUpdate={handleUpdate}
          onShowOnMap={handleShowOnMap}
          displayedRoutes={displayedRoutes}
          onToggleRoute={toggleRouteVisibility}
          onClearRoute={clearRoute}
        />
        <Box flex="1" h="calc(100vh - 74px)">
          <Map
            vehicles={vehicles}
            selectedVehicle={selectedVehicle}
            displayedRoutes={displayedRoutes}
          />
        </Box>
      </Flex>
    </Box>
  );
};

export default Dashboard;
