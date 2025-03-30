import React from "react";
import { VStack } from "@chakra-ui/react";
import VehicleBox from "./VehicleBox";

const VehicleBoxList = ({
  vehicles,
  onDelete,
  onUpdate,
  onShowOnMap,
  onToggleRoute,
  onDeleteRoute,
  onClearRoute,
  displayedRoutes,
}) => {
  return (
    <VStack spacing={2} align="stretch">
      {vehicles.map((veh) => (
        <VehicleBox
          key={veh.id}
          vehicle={veh}
          onDelete={onDelete}
          onUpdate={onUpdate}
          onShowOnMap={onShowOnMap}
          onToggleRoute={onToggleRoute}
          onDeleteRoute={onDeleteRoute}
          onClearRoute={(routeId) => onClearRoute(routeId)}
          displayedRoutes={displayedRoutes}
        />
      ))}
    </VStack>
  );
};

export default VehicleBoxList;
