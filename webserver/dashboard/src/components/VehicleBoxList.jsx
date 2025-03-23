import React from "react";
import { VStack } from "@chakra-ui/react";
import VehicleBox from "./VehicleBox";

const VehicleBoxList = ({ vehicles, onDelete, onUpdate, onShowOnMap }) => {
  return (
    <VStack spacing={2} align="stretch">
      {vehicles.map((veh) => (
        <VehicleBox
          key={veh.id}
          vehicle={veh}
          onDelete={onDelete}
          onUpdate={onUpdate}
          onShowOnMap={onShowOnMap}
        />
      ))}
    </VStack>
  );
};

export default VehicleBoxList;
